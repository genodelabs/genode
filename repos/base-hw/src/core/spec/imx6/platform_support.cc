/*
 * \brief  Specific core implementations
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Martin Stein
 * \date   2014-02-25
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
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

		/* l2 cache controller */
		{ Board::PL310_MMIO_BASE, Board::PL310_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Genode::Arm::User_context::User_context() { cpsr = Psr::init_user(); }


bool Cortex_a9::Board::errata(Cortex_a9::Board::Errata err)
{
	switch (err) {
		case Cortex_a9::Board::ARM_754322:
		case Cortex_a9::Board::ARM_764369:
		case Cortex_a9::Board::ARM_775420:
		case Cortex_a9::Board::PL310_588369:
		case Cortex_a9::Board::PL310_727915:
		case Cortex_a9::Board::PL310_769419:
			return true;
	};
	return false;
}
