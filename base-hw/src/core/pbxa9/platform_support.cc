/*
 * \brief   Parts of platform that are specific to PBXA9
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
#include <drivers/board/pbxa9.h>
#include <drivers/cpu/cortex_a9/core.h>
#include <drivers/pic/pl390_base.h>

/* core includes */
#include <platform.h>

using namespace Genode;


Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Pbxa9::NORTHBRIDGE_DDR_0_BASE, Pbxa9::NORTHBRIDGE_DDR_0_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_irq_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ 0, Pl390_base::MAX_INTERRUPT_ID + 1 }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_irq_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* core timer */
		{ Cortex_a9::PRIVATE_TIMER_IRQ, 1 },

		/* core UART */
		{ Pbxa9::PL011_0_IRQ, 1 }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Pbxa9::SOUTHBRIDGE_APB_BASE, Pbxa9::SOUTHBRIDGE_APB_SIZE },
		{ Pbxa9::NORTHBRIDGE_AHB_BASE, Pbxa9::NORTHBRIDGE_AHB_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* core timer and PIC */
		{ Pbxa9::CORTEX_A9_PRIVATE_MEM_BASE, Pbxa9::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* core UART */
		{ Pbxa9::PL011_0_MMIO_BASE, Pbxa9::PL011_0_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}

