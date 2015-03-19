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

using namespace Genode;

Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ 2*1024*1024, 1024*1024*254 }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ 0x00000000, 0x0001000 },
		{ 0x000a0000, 0x0060000 },
		{ 0xc0000000, 0x1000000 },
		{ 0xfc000000, 0x1000000 },
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


long Platform::irq(long const user_irq)
{
	/* remap IRQ requests to fit I/O APIC configuration */
	if (user_irq) return user_irq + Board::VECTOR_REMAP_BASE;
	return Board::TIMER_VECTOR_USER;
}


Cpu::User_context::User_context() { }
