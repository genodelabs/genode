/*
 * \brief   OKL4 platform interface implementation
 * \author  Norman Feske
 * \date    2009-03-31
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/sleep.h>
#include <util/misc_math.h>
#include <util/xml_generator.h>

/* base-internal includes */
#include <base/internal/crt0.h>
#include <base/internal/stack_area.h>
#include <base/internal/native_utcb.h>
#include <base/internal/globals.h>
#include <base/internal/okl4.h>

/* core includes */
#include <boot_modules.h>
#include <core_log.h>
#include <platform.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <map_local.h>

using namespace Core;


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Mapped_mem_allocator::_map_local(addr_t virt_addr, addr_t phys_addr, size_t size) {
	return map_local(phys_addr, virt_addr, size / get_page_size()); }


bool Mapped_mem_allocator::_unmap_local(addr_t virt_addr, addr_t, size_t size) {
	return unmap_local(virt_addr, size / get_page_size()); }


/**********************
 ** Boot-info parser **
 **********************/

int Core::Platform::bi_init_mem(Okl4::uintptr_t virt_base, Okl4::uintptr_t virt_end,
                                Okl4::uintptr_t phys_base, Okl4::uintptr_t phys_end,
                                const Okl4::bi_user_data_t *data)
{
	Platform &p = *(Platform *)data->user_data;
	p._core_mem_alloc.phys_alloc().add_range(phys_base, phys_end - phys_base + 1);
	p._core_mem_alloc.virt_alloc().add_range(virt_base, virt_end - virt_base + 1);
	return 0;
}


int Core::Platform::bi_add_virt_mem(Okl4::bi_name_t, Okl4::uintptr_t base,
                                    Okl4::uintptr_t end, const Okl4::bi_user_data_t *data)
{
	/* prevent first page from being added to core memory */
	if (base < get_page_size() || end < get_page_size())
		return 0;

	Platform &p = *(Platform *)data->user_data;
	p._core_mem_alloc.virt_alloc().add_range(base, end - base + 1);
	return 0;
}


int Core::Platform::bi_add_phys_mem(Okl4::bi_name_t pool, Okl4::uintptr_t base,
                                    Okl4::uintptr_t end, const Okl4::bi_user_data_t *data)
{
	if (pool == 2) {
		Platform &p = *(Platform *)data->user_data;
		p._core_mem_alloc.phys_alloc().add_range(base, end - base + 1);
	}
	return 0;
}


static char init_slab_block_rom[get_page_size()];
static char init_slab_block_thread[get_page_size()];


Core::Platform::Platform()
:
	_io_mem_alloc(&core_mem_alloc()), _io_port_alloc(&core_mem_alloc()),
	_irq_alloc(&core_mem_alloc()),
	_rom_slab(&core_mem_alloc(), &init_slab_block_rom),
	_thread_slab(core_mem_alloc(), &init_slab_block_thread)
{
	/*
	 * We must be single-threaded at this stage and so this is safe.
	 */
	static bool initialized = 0;
	if (initialized) panic("Platform constructed twice!");
	initialized = true;

	/*
	 * Determine address of boot-info structure. On startup, the OKL4 kernel
	 * provides this address in roottask's UTCB message register 1.
	 */
	Okl4::L4_Word_t boot_info_addr;
	Okl4::L4_StoreMR(1, &boot_info_addr);

	/*
	 * Request base address for UTCB locations
	 */
	_utcb_base = (addr_t)Okl4::utcb_base_get();

	/*
	 * Define our own thread ID
	 */
	Okl4::__L4_TCR_Set_ThreadWord(UTCB_TCR_THREAD_WORD_MYSELF, Okl4::L4_rootserver.raw);

	/*
	 * By default, the first roottask thread is executed at maxiumum priority.
	 * To make preemptive scheduler work as expected, we set the priority of
	 * ourself to the default priority of all other threads, which is 100 on
	 * OKL4.
	 */
	L4_Set_Priority(Okl4::L4_Myself(), Platform_thread::DEFAULT_PRIORITY);

	/*
	 * Invoke boot-info parser for determining the memory configuration and
	 * the location of the boot modules.
	 */

	/*
	 * Initialize callback function for parsing the boot-info
	 *
	 * The supplied callback functions differ slightly from the interface
	 * used by the boot-info library in that they do not have a return
	 * type.
	 */
	static Okl4::bi_callbacks_t callbacks;
	callbacks.init_mem      = bi_init_mem;
	callbacks.add_virt_mem  = bi_add_virt_mem;
	callbacks.add_phys_mem  = bi_add_phys_mem;

	Okl4::bootinfo_parse((void *)boot_info_addr, &callbacks, this);

	/* initialize interrupt allocator */
	_irq_alloc.add_range(0, 0x10);

	/* I/O memory could be the whole user address space */
	_io_mem_alloc.add_range(0, ~0);

	/* I/O port allocator (only meaningful for x86) */
	_io_port_alloc.add_range(0, 0x10000);

	_init_rom_modules();

	/* preserve stack area in core's virtual address space */
	_core_mem_alloc.virt_alloc().remove_range(stack_area_virtual_base(),
	                                          stack_area_virtual_size());

	_vm_start = 0x1000;
	_vm_size  = 0xc0000000 - _vm_start;

	log(_rom_fs);

	/* setup task object for core task */
	_core_pd = new (core_mem_alloc()) Platform_pd(true);

	/*
	 * We setup the thread object for thread0 in core task using a special
	 * interface that allows us to specify the thread ID. For core this creates
	 * the situation that task_id == thread_id of first task. But since we do
	 * not destroy this task, it should be no problem.
	 */
	Platform_thread &core_thread =
		*new (&_thread_slab) Platform_thread("core.main");

	core_thread.set_l4_thread_id(Okl4::L4_rootserver);

	_core_pd->bind_thread(core_thread);

	/* core log as ROM module */
	{
		unsigned const pages  = 1;
		size_t const log_size = pages << get_page_size_log2();
		unsigned const align  = get_page_size_log2();

		ram_alloc().alloc_aligned(log_size, align).with_result(

			[&] (void *phys) {
				addr_t const phys_addr = reinterpret_cast<addr_t>(phys);

				region_alloc().alloc_aligned(log_size, align). with_result(

					[&] (void *ptr) {

						map_local(phys_addr, (addr_t)ptr, pages);
						memset(ptr, 0, log_size);

						new (core_mem_alloc())
							Rom_module(_rom_fs, "core_log", phys_addr, log_size);

						init_core_log(Core_log_range { (addr_t)ptr, log_size } );
					},
					[&] (Range_allocator::Alloc_error) { }
				);
			},
			[&] (Range_allocator::Alloc_error) { }
		);
	}

	/* export platform-specific infos */
	{
		unsigned const pages  = 1;
		size_t   const size   = pages << get_page_size_log2();

		ram_alloc().alloc_aligned(size, get_page_size_log2()).with_result(

			[&] (void *phys_ptr) {
				addr_t const phys_addr = reinterpret_cast<addr_t>(phys_ptr);

				/* let one page free after the log buffer */
				region_alloc().alloc_aligned(size, get_page_size_log2()).with_result(

					[&] (void *core_local_ptr) {
						addr_t const core_local_addr = reinterpret_cast<addr_t>(core_local_ptr);

						if (map_local(phys_addr, core_local_addr, pages)) {

							Xml_generator xml(reinterpret_cast<char *>(core_local_addr),
							                  size, "platform_info", [&] () {
								xml.node("kernel", [&] () { xml.attribute("name", "okl4"); });
							});

							new (core_mem_alloc())
								Rom_module(_rom_fs, "platform_info", phys_addr, size);
						}
					},
					[&] (Range_allocator::Alloc_error) { }
				);
			},
			[&] (Range_allocator::Alloc_error) { }
		);
	}
}


/********************************
 ** Generic platform interface **
 ********************************/

void Core::Platform::wait_for_exit()
{
	/*
	 * On OKL4, core never exits. So let us sleep forever.
	 */
	sleep_forever();
}
