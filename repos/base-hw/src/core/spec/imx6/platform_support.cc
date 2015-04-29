/*
 * \brief  Specific core implementations
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Martin Stein
 * \date   2014-02-25
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <cpu.h>

using namespace Genode;

Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::RAM0_BASE, Board::RAM0_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::MMIO_BASE, Board::MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* core UART */
		{ Board::UART_1_MMIO_BASE, Board::UART_1_MMIO_SIZE },

		/* CPU-local core MMIO like interrupt controller and timer */
		{ Board::CORTEX_A9_PRIVATE_MEM_BASE, Board::CORTEX_A9_PRIVATE_MEM_SIZE },

	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


bool Imx::Board::is_smp() { return true; }

Cpu::User_context::User_context() { cpsr = Psr::init_user(); }
