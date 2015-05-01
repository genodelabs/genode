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

Platform::Platform() :
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()), _vm_base(0), _vm_size(0)
{
	/* initialize core allocators */

	/* remove core image from core's virtual address allocator */

	/* preserve context area in core's virtual address space */

	/* remove used core memory from physical memory allocator */

	/* add boot modules to ROM fs */

	/* initialize interrupt allocator */
	_irq_alloc.add_range(0, 255);

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
