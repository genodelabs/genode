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
#include <drivers/board/vea9x4.h>
#include <drivers/cpu/cortex_a9/core.h>
#include <drivers/pic/pl390_base.h>

/* Core includes */
#include <platform.h>

using namespace Genode;


Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Vea9x4::LOCAL_DDR2_BASE, Vea9x4::LOCAL_DDR2_SIZE }
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
		/* Core timer */
		{ Cortex_a9::PRIVATE_TIMER_IRQ, 1 },

		/* Core UART */
		{ Vea9x4::PL011_0_IRQ, 1 }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Vea9x4::SMB_CS7_BASE, Vea9x4::SMB_CS7_SIZE },
		{ Vea9x4::SMB_CS0_TO_CS6_BASE, Vea9x4::SMB_CS0_TO_CS6_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* Core timer and PIC */
		{ Vea9x4::CORTEX_A9_PRIVATE_MEM_BASE,
		  Vea9x4::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* Core UART */
		{ Vea9x4::PL011_0_MMIO_BASE, Vea9x4::PL011_0_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}

