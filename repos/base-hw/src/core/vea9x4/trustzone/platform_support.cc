/*
 * \brief   Platform implementations specific for base-hw and VEA9X4
 * \author  Martin Stein
 * \date    2012-04-27
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <drivers/trustzone.h>

/* core includes */
#include <board.h>
#include <processor_driver.h>
#include <platform.h>
#include <pic/vea9x4_trustzone.h>

using namespace Genode;

Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Trustzone::SECURE_RAM_BASE, Trustzone::SECURE_RAM_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::MMIO_0_BASE, Board::MMIO_0_SIZE },
		{ Board::MMIO_1_BASE, Board::MMIO_1_SIZE },
		{ Trustzone::NONSECURE_RAM_BASE, Trustzone::NONSECURE_RAM_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* Core timer and PIC */
		{ Board::CORTEX_A9_PRIVATE_MEM_BASE,
		  Board::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* Core UART */
		{ Board::PL011_0_MMIO_BASE, Board::PL011_0_MMIO_SIZE },

		/* vm state memory */
		{ Trustzone::VM_STATE_BASE, Trustzone::VM_STATE_SIZE },
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Processor_driver::User_context::User_context() { cpsr = Psr::init_user_with_trustzone(); }
