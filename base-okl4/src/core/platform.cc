/*
 * \brief   OKL4 platform interface implementation
 * \author  Norman Feske
 * \date    2009-03-31
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/allocator_avl.h>
#include <base/crt0.h>
#include <base/sleep.h>
#include <util/misc_math.h>

/* core includes */
#include <core_parent.h>
#include <platform.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <map_local.h>

/* OKL4 includes */
namespace Okl4 {
#include <l4/ipc.h>
#include <l4/schedule.h>
}

using namespace Genode;


static const bool verbose_boot_info = false;

enum { MAX_BOOT_MODULES = 64 };
enum { MAX_BOOT_MODULE_NAME_LEN = 32 };
static struct
{
	char name[MAX_BOOT_MODULE_NAME_LEN];
	addr_t base;
	size_t size;
} boot_modules[MAX_BOOT_MODULES];

static int num_boot_module_memsects;
static int num_boot_module_objects;


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Core_mem_allocator::Mapped_mem_allocator::_map_local(addr_t   virt_addr,
                                                          addr_t   phys_addr,
                                                          unsigned size_log2)
{
	return map_local(phys_addr, virt_addr, 1 << (size_log2 - get_page_size_log2()));
}


/**********************
 ** Boot-info parser **
 **********************/

int Platform::bi_init_mem(Okl4::uintptr_t virt_base, Okl4::uintptr_t virt_end,
                          Okl4::uintptr_t phys_base, Okl4::uintptr_t phys_end,
                          const Okl4::bi_user_data_t *data)
{
	if (verbose_boot_info)
		printf("init_mem: virt=[%08lx,%08lx), phys=[%08lx,%08lx)\n",
		       virt_base, virt_end, phys_base, phys_end);

	Platform *p = (Platform *)data->user_data;
	p->_core_mem_alloc.phys_alloc()->add_range(phys_base, phys_end - phys_base + 1);
	p->_core_mem_alloc.virt_alloc()->add_range(virt_base, virt_end - virt_base + 1);
	return 0;
}


int Platform::bi_add_virt_mem(Okl4::bi_name_t pool, Okl4::uintptr_t base,
                              Okl4::uintptr_t end, const Okl4::bi_user_data_t *data)
{
	if (verbose_boot_info)
		printf("add_virt_mem: pool=%d region=[0x%08lx,0x%08lx], %ld pages\n",
		       pool, base, end, (end - base + 1)/4096);

	/* prevent first page from being added to core memory */
	if (base < get_page_size() || end < get_page_size())
		return 0;

	Platform *p = (Platform *)data->user_data;
	p->_core_mem_alloc.virt_alloc()->add_range(base, end - base + 1);
	return 0;
}


int Platform::bi_add_phys_mem(Okl4::bi_name_t pool, Okl4::uintptr_t base,
                              Okl4::uintptr_t end, const Okl4::bi_user_data_t *data)
{
	if (verbose_boot_info)
		printf("add_phys_mem: pool=%d region=[0x%08lx,0x%08lx], %ld pages\n",
		       pool, base, end, (end - base + 1)/4096);

	if (pool == 2) {
		Platform *p = (Platform *)data->user_data;
		p->_core_mem_alloc.phys_alloc()->add_range(base, end - base + 1);
	}
	return 0;
}


int Platform::bi_export_object(Okl4::bi_name_t pd, Okl4::bi_name_t obj,
                               Okl4::bi_export_type_t export_type, char *key,
                               Okl4::size_t key_len, const Okl4::bi_user_data_t * data)
{
	if (verbose_boot_info)
		printf("export_object: pd=%d obj=%d type=%d key=\"%s\"\n",
		       pd, obj, export_type, key);

	/*
	 * We walk the boot info only once and collect all memory section
	 * objects. Each time we detect a memory section outside of roottask
	 * (PD 0), we increment the boot module index.
	 */

	/* reset module index (roottask objects appear before other pd's objects) */
	if (pd == 0) num_boot_module_objects = 0;

	if (export_type != Okl4::BI_EXPORT_MEMSECTION_CAP)
		return 0;

	if (num_boot_module_objects >= MAX_BOOT_MODULES) {
		PERR("Maximum number of boot modules exceeded");
		return -1;
	}

	/* copy name from object key */
	key_len = min((int)key_len, MAX_BOOT_MODULE_NAME_LEN - 1);
	for (unsigned i = 0; i < key_len; i++) {

		/* convert letter to lower-case */
		char c = key[i];
		if (c >= 'A' && c <= 'Z')
			c -= 'A' - 'a';

		boot_modules[num_boot_module_objects].name[i] = c;
	}
	/* null-terminate string */
	boot_modules[num_boot_module_objects].name[key_len] = 0;

	num_boot_module_objects++;
	return 0;
}


Okl4::bi_name_t Platform::bi_new_ms(Okl4::bi_name_t owner,
                                    Okl4::uintptr_t base, Okl4::uintptr_t size,
                                    Okl4::uintptr_t flags, Okl4::uintptr_t attr,
                                    Okl4::bi_name_t physpool, Okl4::bi_name_t virtpool,
                                    Okl4::bi_name_t zone, const Okl4::bi_user_data_t *data)
{
	if (verbose_boot_info)
		printf("new_ms: owner=%d region=[%lx,%lx), flags=%lx, attr=%lx, physpool=%d, virtpool=%d, zone=%d\n",
		       owner, base, base + size - 1, flags, attr, physpool, virtpool, zone);

	/* reset module index (see comment in 'bi_export_object') */
	if (owner == 0) num_boot_module_memsects = 0;

	/* ignore memory pools other than pool 3 (this is just a heuristic) */
	if (virtpool != 3) return 0;

	if (num_boot_module_memsects >= MAX_BOOT_MODULES) {
		PERR("Maximum number of boot modules exceeded");
		return -1;
	}

	boot_modules[num_boot_module_memsects].base = base;
	boot_modules[num_boot_module_memsects].size = size;

	num_boot_module_memsects++;
	return 0;
}


Platform::Platform() :
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc())
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

	printf("parsing boot info at 0x%p...\n", (void *)boot_info_addr);

	/*
	 * Initialize callback function for parsing the boot-info
	 *
	 * The supplied callback functions differ slightly from the interface
	 * used by the boot-info library in that they do not have a return
	 * type.
	 */
	static Okl4::bi_callbacks_t callbacks;
	callbacks.init_mem      = Platform::bi_init_mem;
	callbacks.add_virt_mem  = Platform::bi_add_virt_mem;
	callbacks.add_phys_mem  = Platform::bi_add_phys_mem;
	callbacks.export_object = Platform::bi_export_object;
	callbacks.new_ms        = Platform::bi_new_ms;

	Okl4::bootinfo_parse((void *)boot_info_addr, &callbacks, this);

	/* make gathered boot-module info known to '_rom_fs' */
	int num_boot_modules = min(num_boot_module_objects, num_boot_module_memsects);
	for (int i = 0; i < num_boot_modules; i++) {
		Rom_module *r = new (core_mem_alloc())
		                Rom_module(boot_modules[i].base,
		                           boot_modules[i].size,
		                           boot_modules[i].name);
		_rom_fs.insert(r);
	}

	/* initialize interrupt allocator */
	_irq_alloc.add_range(0, 0x10);

	/* I/O memory could be the whole user address space */
	_io_mem_alloc.add_range(0, ~0);

	/* I/O port allocator (only meaningful for x86) */
	_io_port_alloc.add_range(0, 0x10000);

	/* preserve context area in core's virtual address space */
	_core_mem_alloc.virt_alloc()->remove_range(Native_config::context_area_virtual_base(),
	                                           Native_config::context_area_virtual_size());

	_vm_start = 0x1000;
	_vm_size  = 0xb0000000 - 0x1000;

	/*
	 * When dumping 'ram_alloc', there are several small blocks in addition
	 * to the available free memory visible. These small blocks are used to
	 * hold the meta data for the ROM modules as initialized by '_setup_rom'.
	 */
	if (verbose_boot_info) {
		printf(":phys_alloc: ");   _core_mem_alloc.phys_alloc()->raw()->dump_addr_tree();
		printf(":virt_alloc: ");   _core_mem_alloc.virt_alloc()->raw()->dump_addr_tree();
		printf(":io_mem: ");       _io_mem_alloc.raw()->dump_addr_tree();
		printf(":io_port: ");      _io_port_alloc.raw()->dump_addr_tree();
		printf(":irq: ");          _irq_alloc.raw()->dump_addr_tree();
		printf(":rom_fs: ");       _rom_fs.print_fs();
	}

	/* setup task object for core task */
	_core_pd = new(core_mem_alloc()) Platform_pd(true);

	/*
	 * We setup the thread object for thread0 in core task using a special
	 * interface that allows us to specify the thread ID. For core this creates
	 * the situation that task_id == thread_id of first task. But since we do
	 * not destroy this task, it should be no problem.
	 */
	Platform_thread *core_thread =
		new(core_mem_alloc()) Platform_thread("core.main");

	core_thread->set_l4_thread_id(Okl4::L4_rootserver);

	_core_pd->bind_thread(core_thread);
}


/********************************
 ** Generic platform interface **
 ********************************/

void Platform::wait_for_exit()
{
	/*
	 * On OKL4, core never exits. So let us sleep forever.
	 */
	sleep_forever();
}


void Core_parent::exit(int exit_value) { }
