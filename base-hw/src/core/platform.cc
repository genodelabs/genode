/*
 * \brief   Platform implementation specific for hw
 * \author  Martin Stein
 * \date    2011-12-21
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <kernel/log.h>

/* core includes */
#include <core_parent.h>
#include <platform.h>
#include <util.h>

using namespace Genode;

extern int _prog_img_beg;
extern int _prog_img_end;

/**
 * Format of a boot-module header
 */
struct Bm_header
{
	long name; /* physical address of null-terminated string */
	long base; /* physical address of module data */
	long size; /* size of module data in bytes */
};

extern int       _boot_modules_begin;
extern Bm_header _boot_module_headers_begin;
extern Bm_header _boot_module_headers_end;
extern int       _boot_modules_end;

/**
 * Functionpointer that provides accessor to a pool of address regions
 */
typedef Native_region * (*Region_pool)(unsigned const);

namespace Kernel
{
	addr_t mode_transition_virt_base();
	size_t mode_transition_size();
}


/**
 * Helper to initialise allocators through include/exclude region lists
 */
static void init_alloc(Range_allocator * const alloc,
                       Region_pool incl_regions, Region_pool excl_regions,
                       unsigned const granu_log2 = 0)
{
	/* make all include regions available */
	Native_region * r = incl_regions(0);
	for (unsigned i = 0; r; r = incl_regions(++i)) {
		if (granu_log2) {
			addr_t const b = trunc(r->base, granu_log2);
			addr_t const s = round(r->size, granu_log2);
			alloc->add_range(b, s);
		}
		else alloc->add_range(r->base, r->size);
	}
	/* preserve all exclude regions */
	r = excl_regions(0);
	for (unsigned i = 0; r; r = excl_regions(++i)) {
		if (granu_log2) {
			addr_t const b = trunc(r->base, granu_log2);
			addr_t const s = round(r->size, granu_log2);
			alloc->remove_range(b, s);
		}
		else alloc->remove_range(r->base, r->size);
	}
}


/**************
 ** Platform **
 **************/

Native_region * Platform::_core_only_ram_regions(unsigned const i)
{
	static Native_region _r[] =
	{
		/* avoid null pointers */
		{ 0, 1 },

		/* mode transition region */
		{ Kernel::mode_transition_virt_base(), Kernel::mode_transition_size() },

		/* core image */
		{ (addr_t)&_prog_img_beg,
		  (size_t)((addr_t)&_prog_img_end - (addr_t)&_prog_img_beg) },

		/* boot modules */
		{ (addr_t)&_boot_modules_begin,
		  (size_t)((addr_t)&_boot_modules_end - (addr_t)&_boot_modules_begin) }
	};
	return i < sizeof(_r)/sizeof(_r[0]) ? &_r[i] : 0;
}


Platform::Platform() :
	_core_mem_alloc(0),
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()), _vm_base(0x1000), _vm_size(0xfffef000)
{
	/*
	 * Initialise platform resource allocators.
	 * Core mem alloc must come first because it is
	 * used by the other allocators.
	 */
	enum { VERBOSE = 0 };
	unsigned const psl2 = get_page_size_log2();
	init_alloc(&_core_mem_alloc, _ram_regions, _core_only_ram_regions, psl2);
	init_alloc(&_irq_alloc, _irq_regions, _core_only_irq_regions);
	init_alloc(&_io_mem_alloc, _mmio_regions, _core_only_mmio_regions, psl2);

	/* add boot modules to ROM FS */
	Bm_header * header = &_boot_module_headers_begin;
	for (; header < &_boot_module_headers_end; header++) {
		Rom_module * rom_module = new (core_mem_alloc())
			Rom_module(header->base, header->size, (const char*)header->name);
		_rom_fs.insert(rom_module);
	}
	/* print ressource summary */
	if (VERBOSE) {
		printf("Core memory allocator\n");
		printf("---------------------\n");
		_core_mem_alloc.raw()->dump_addr_tree();
		printf("\n");
		printf("IO memory allocator\n");
		printf("-------------------\n");
		_io_mem_alloc.raw()->dump_addr_tree();
		printf("\n");
		printf("IRQ allocator\n");
		printf("-------------------\n");
		_irq_alloc.raw()->dump_addr_tree();
		printf("\n");
		printf("ROM filesystem\n");
		printf("--------------\n");
		_rom_fs.print_fs();
		printf("\n");
	}
}


/*****************
 ** Core_parent **
 *****************/

void Core_parent::exit(int exit_value)
{
	kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
	while (1) ;
}

