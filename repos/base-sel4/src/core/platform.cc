/*
 * \brief   Platform interface implementation
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>

/* core includes */
#include <core_parent.h>
#include <platform.h>
#include <map_local.h>

/* seL4 includes */
#include <sel4/bootinfo.h>
#include <sel4/interfaces/sel4_client.h>

using namespace Genode;

static bool const verbose_boot_info = true;


/*
 * Memory-layout information provided by the linker script
 */

/* virtual address range consumed by core's program image */
extern unsigned _prog_img_beg, _prog_img_end;


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Core_mem_allocator::Mapped_mem_allocator::_map_local(addr_t virt_addr,
                                                          addr_t phys_addr,
                                                          unsigned size)
{
	return map_local(phys_addr, virt_addr, size / get_page_size());
}


bool Core_mem_allocator::Mapped_mem_allocator::_unmap_local(addr_t virt_addr,
                                                            unsigned size)
{
	return unmap_local(virt_addr, size / get_page_size());
}


/************************
 ** Platform interface **
 ************************/


/**
 * Obtain seL4 boot info structure
 */
static seL4_BootInfo const &sel4_boot_info()
{
	extern Genode::addr_t __initial_bx;
	return *(seL4_BootInfo const *)__initial_bx;
}


/**
 * Initialize allocator with untyped memory ranges according to the boot info
 */
static void init_allocator(Range_allocator &alloc, seL4_BootInfo const &bi,
                           unsigned const start_idx, unsigned const num_idx)
{
	for (unsigned i = start_idx; i < start_idx + num_idx; i++) {

		/* index into 'untypedPaddrList' and 'untypedSizeBitsList' */
		unsigned const k = i - bi.untyped.start;

		addr_t const base = bi.untypedPaddrList[k];
		size_t const size = 1UL << bi.untypedSizeBitsList[k];

		alloc.add_range(base, size);
	}
}


Platform::Platform()
:
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()),
	_vm_base(0x1000),
	_vm_size(2*1024*1024*1024UL - _vm_base) /* use the lower 2GiB */
{
	/*
	 * Initialize core allocators
	 */
	seL4_BootInfo const &bi = sel4_boot_info();

	/* interrupt allocator */
	_irq_alloc.add_range(0, 255);

	/* physical memory ranges */
	init_allocator(*_core_mem_alloc.phys_alloc(), bi, bi.untyped.start,
	               bi.untyped.end - bi.untyped.start);

	/* I/O memory ranges */
	init_allocator(_io_mem_alloc, bi, bi.deviceUntyped.start,
	               bi.deviceUntyped.end - bi.deviceUntyped.start);

	/* core's virtual memory */
	_core_mem_alloc.virt_alloc()->add_range(_vm_base, _vm_size);

	/* remove core image from core's virtual address allocator */
	addr_t const core_virt_beg = trunc_page((addr_t)&_prog_img_beg),
	             core_virt_end = round_page((addr_t)&_prog_img_end);
	size_t const core_size     = core_virt_end - core_virt_beg;

	_core_mem_alloc.virt_alloc()->remove_range(core_virt_beg, core_size);

	if (verbose_boot_info) {
		printf("core image:\n");
		printf("  virtual address range [%08lx,%08lx) size=0x%zx\n",
		       core_virt_beg, core_virt_end, core_size);
	}

	/* preserve context area in core's virtual address space */
	_core_mem_alloc.virt_alloc()->remove_range(Native_config::context_area_virtual_base(),
	                                           Native_config::context_area_virtual_size());

	/* add boot modules to ROM fs */

	/*
	 * Print statistics about allocator initialization
	 */
	printf("VM area at [%08lx,%08lx)\n", _vm_base, _vm_base + _vm_size);

	if (verbose_boot_info) {
		printf(":phys_alloc:   "); _core_mem_alloc.phys_alloc()->raw()->dump_addr_tree();
		printf(":virt_alloc:   "); _core_mem_alloc.virt_alloc()->raw()->dump_addr_tree();
		printf(":io_mem_alloc: "); _io_mem_alloc.raw()->dump_addr_tree();
	}
}


void Platform::wait_for_exit()
{
	sleep_forever();
}


void Core_parent::exit(int exit_value) { }
