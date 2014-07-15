/*
 * \brief   Platform implementations specific for base-hw and Raspberry Pi
 * \author  Norman Feske
 * \date    2013-04-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
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
		{ Board::RAM_0_BASE, Board::RAM_0_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ 0x20000000, 0x22000000 },
		{ 0x50000000, 0x10000000 },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* UART */
		{ Board::PL011_0_MMIO_BASE, Board::PL011_0_MMIO_SIZE },

		/* system timer */
		{ Board::SYSTEM_TIMER_MMIO_BASE, Board::SYSTEM_TIMER_MMIO_SIZE },

		/* IRQ controller */
		{ Board::IRQ_CONTROLLER_BASE, Board::IRQ_CONTROLLER_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Cpu::User_context::User_context() { cpsr = Psr::init_user(); }
