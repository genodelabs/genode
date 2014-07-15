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
#include <cpu.h>
#include <platform.h>
#include <pic.h>
#include <trustzone.h>
#include <pic.h>

using namespace Genode;

/* monitor exception vector address */
extern int _mon_kernel_entry;


void Kernel::init_trustzone(Pic * pic)
{
	/* check for compatibility */
	if (PROCESSORS > 1) {
		PERR("trustzone not supported with multiprocessing");
		return;
	}
	/* set exception vector entry */
	Cpu::mon_exception_entry_at((Genode::addr_t)&_mon_kernel_entry);

	/* enable coprocessor access for TZ VMs */
	Cpu::allow_coprocessor_nonsecure();

	/* set unsecure IRQs */
	pic->unsecure(34); //Timer 0/1
	pic->unsecure(35); //Timer 2/3
	pic->unsecure(36); //RTC
	pic->unsecure(37); //UART0
	pic->unsecure(41); //MCI0
	pic->unsecure(42); //MCI1
	pic->unsecure(43); //AACI
	pic->unsecure(44); //KMI0
	pic->unsecure(45); //KMI1
	pic->unsecure(47); //ETHERNET
	pic->unsecure(48); //USB
}


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
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Cpu::User_context::User_context() { cpsr = Psr::init_user_with_trustzone(); }
