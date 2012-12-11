/*
 * \brief   Platform implementations specific for base-hw and VEA9X4
 * \author  Martin Stein
 * \date    2012-04-27
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <drivers/board_base.h>

/* Core includes */
#include <platform.h>
#include <cortex_a9/cpu.h>
#include <cortex_a9/no_trustzone/pic.h>

using namespace Genode;


Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board_base::RAM_0_BASE, Board_base::RAM_0_SIZE },
		{ Board_base::RAM_1_BASE, Board_base::RAM_1_SIZE },
		{ Board_base::RAM_2_BASE, Board_base::RAM_2_SIZE },
		{ Board_base::RAM_3_BASE, Board_base::RAM_3_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_irq_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ 0, Cortex_a9_no_trustzone::Pic::MAX_INTERRUPT_ID + 1 }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_irq_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* Core timer */
		{ Cortex_a9::Cpu::PRIVATE_TIMER_IRQ, 1 },

		/* Core UART */
		{ Board_base::PL011_0_IRQ, 1 }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board_base::MMIO_0_BASE, Board_base::MMIO_0_SIZE },
		{ Board_base::MMIO_1_BASE, Board_base::MMIO_1_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* Core timer and PIC */
		{ Board_base::CORTEX_A9_PRIVATE_MEM_BASE,
		  Board_base::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* Core UART */
		{ Board_base::PL011_0_MMIO_BASE, Board_base::PL011_0_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}
