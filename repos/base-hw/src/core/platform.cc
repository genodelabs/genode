/*
 * \brief   Platform implementation specific for hw
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2011-12-21
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
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
#include <map_local.h>
#include <platform.h>
#include <platform_pd.h>
#include <page_flags.h>
#include <util.h>
#include <pic.h>
#include <kernel/kernel.h>
#include <translation_table.h>
#include <trustzone.h>

using namespace Genode;

extern int _prog_img_beg;
extern int _prog_img_end;

void __attribute__((weak)) Kernel::init_trustzone(Pic * pic) { }

/**
 * Format of a boot-module header
 */
struct Bm_header
{
	long name; /* physical address of null-terminated string */
	long base; /* physical address of module data */
	long size; /* size of module data in bytes */
};

extern Bm_header _boot_modules_headers_begin;
extern Bm_header _boot_modules_headers_end;
extern int       _boot_modules_binaries_begin;
extern int       _boot_modules_binaries_end;


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

addr_t Platform::core_translation_tables()
{
	size_t sz = max((size_t)Translation_table::TABLE_LEVEL_X_SIZE_LOG2,
	                get_page_size_log2());
	return align_addr<addr_t>((addr_t)&_boot_modules_binaries_end, sz);
}


Native_region * Platform::_core_only_ram_regions(unsigned const i)
{
	static Native_region _r[] =
	{
		/* core image */
		{ (addr_t)&_prog_img_beg,
		  (size_t)((addr_t)&_prog_img_end - (addr_t)&_prog_img_beg) },

		/* boot modules */
		{ (addr_t)&_boot_modules_binaries_begin,
		  (size_t)((addr_t)&_boot_modules_binaries_end -
		           (addr_t)&_boot_modules_binaries_begin) },

		/* translation table allocator */
		{ core_translation_tables(), core_translation_tables_size() }
	};
	return i < sizeof(_r)/sizeof(_r[0]) ? &_r[i] : 0;
}

static Native_region * virt_region(unsigned const i) {
	static Native_region r = { VIRT_ADDR_SPACE_START, VIRT_ADDR_SPACE_SIZE };
	return i ? 0 : &r; }


Platform::Platform()
:
	_io_mem_alloc(core_mem_alloc()),
	_io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()),
	_vm_start(VIRT_ADDR_SPACE_START), _vm_size(VIRT_ADDR_SPACE_SIZE)
{
	/*
	 * Initialise platform resource allocators.
	 * Core mem alloc must come first because it is
	 * used by the other allocators.
	 */
	enum { VERBOSE = 0 };
	init_alloc(_core_mem_alloc.phys_alloc(), _ram_regions,
	           _core_only_ram_regions, get_page_size_log2());
	init_alloc(_core_mem_alloc.virt_alloc(), virt_region,
	           _core_only_ram_regions, get_page_size_log2());

	_init_io_port_alloc();

	/* make all non-kernel interrupts available to the interrupt allocator */
	for (unsigned i = 0; i < Kernel::Pic::NR_OF_IRQ; i++) {
		if (i == Timer::interrupt_id(i) || i == Pic::IPI)
			continue;
		_irq_alloc.add_range(i, 1);
	}

	_init_io_mem_alloc();

	/* add boot modules to ROM FS */
	Bm_header * header = &_boot_modules_headers_begin;
	for (; header < &_boot_modules_headers_end; header++) {
		Rom_module * rom_module = new (core_mem_alloc())
			Rom_module(header->base, header->size, (const char*)header->name);
		_rom_fs.insert(rom_module);
	}

	/* print ressource summary */
	if (VERBOSE) {
		printf("Core virtual memory allocator\n");
		printf("---------------------\n");
		_core_mem_alloc.virt_alloc()->raw()->dump_addr_tree();
		printf("\n");
		printf("RAM memory allocator\n");
		printf("---------------------\n");
		_core_mem_alloc.phys_alloc()->raw()->dump_addr_tree();
		printf("\n");
		printf("IO memory allocator\n");
		printf("-------------------\n");
		_io_mem_alloc.raw()->dump_addr_tree();
		printf("\n");
		printf("IO port allocator\n");
		printf("-------------------\n");
		_io_port_alloc.raw()->dump_addr_tree();
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
	Kernel::log() << __PRETTY_FUNCTION__ << "not implemented\n";
	while (1) ;
}


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Genode::map_local(addr_t from_phys, addr_t to_virt, size_t num_pages,
                       Page_flags flags)
{
	Platform_pd * pd = Kernel::core_pd()->platform_pd();
	return pd->insert_translation(to_virt, from_phys,
	                              num_pages * get_page_size(), flags);
}


bool Genode::unmap_local(addr_t virt_addr, size_t num_pages)
{
	Platform_pd * pd = Kernel::core_pd()->platform_pd();
	pd->flush(virt_addr, num_pages * get_page_size());
	return true;
}


bool Core_mem_allocator::Mapped_mem_allocator::_map_local(addr_t   virt_addr,
                                                          addr_t   phys_addr,
                                                          unsigned size) {
	return ::map_local(phys_addr, virt_addr, size / get_page_size()); }


bool Core_mem_allocator::Mapped_mem_allocator::_unmap_local(addr_t   virt_addr,
                                                            unsigned size) {
	return ::unmap_local(virt_addr, size / get_page_size()); }
