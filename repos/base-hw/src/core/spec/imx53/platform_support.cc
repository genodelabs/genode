/*
 * \brief   Specific core implementations
 * \author  Stefan Kalkowski
 * \date    2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <pic.h>
#include <cpu.h>

using namespace Genode;

Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::RAM0_BASE, Board::RAM0_SIZE },
		{ Board::RAM1_BASE, Board::RAM1_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ 0x07000000, 0x1000000  }, /* security controller */
		{ 0x10000000, 0x30000000 }, /* SATA, IPU, GPU      */
		{ 0x50000000, 0x20000000 }, /* Misc.               */
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* core UART */
		{ Board::UART_1_MMIO_BASE, Board::UART_1_MMIO_SIZE },

		/* core timer */
		{ Board::EPIT_1_MMIO_BASE, Board::EPIT_1_MMIO_SIZE },

		/* interrupt controller */
		{ Board::IRQ_CONTROLLER_BASE, Board::IRQ_CONTROLLER_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Cpu::User_context::User_context() { cpsr = Psr::init_user(); }
