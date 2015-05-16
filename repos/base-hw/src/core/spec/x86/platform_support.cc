/*
 * \brief   Platform implementations specific for x86
 * \author  Norman Feske
 * \author  Reto Buerki
 * \date    2013-04-05
 *
 * XXX dimension allocators according to the available physical memory
 */

/*
 * Copyright (C) 2013-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <cpu.h>
#include <pic.h>
#include <kernel/kernel.h>

using namespace Genode;

Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ 2*1024*1024, 1024*1024*254 }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::MMIO_LAPIC_BASE,  Board::MMIO_LAPIC_SIZE  },
		{ Board::MMIO_IOAPIC_BASE, Board::MMIO_IOAPIC_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


void Platform::_init_io_port_alloc()
{
	_io_port_alloc.add_range(0, 0x10000);
}


/**
 * Remove given exclude memory regions from specified allocator.
 */
static void alloc_exclude_regions(Range_allocator * const alloc,
                                  Region_pool excl_regions)
{
	Native_region * r = excl_regions(0);
	for (unsigned i = 0; r; r = excl_regions(++i))
		alloc->remove_range(r->base, r->size);
}


void Platform::_init_io_mem_alloc()
{
	/* add entire adress space minus the RAM memory regions */
	_io_mem_alloc.add_range(0, ~0x0UL);
	alloc_exclude_regions(&_io_mem_alloc, _ram_regions);
	alloc_exclude_regions(&_io_mem_alloc, _core_only_ram_regions);
}


long Platform::irq(long const user_irq)
{
	/* remap IRQ requests to fit I/O APIC configuration */
	if (user_irq) return user_irq + Board::VECTOR_REMAP_BASE;
	return Board::TIMER_VECTOR_USER;
}


void Platform::setup_irq_mode(unsigned irq_number, unsigned trigger,
                              unsigned polarity)
{
	Kernel::pic()->ioapic.setup_irq_mode(irq_number, trigger, polarity);
}
