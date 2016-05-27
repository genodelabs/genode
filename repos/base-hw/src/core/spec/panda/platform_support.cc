/*
 * \brief   Platform implementations specific for base-hw and Panda A2
 * \author  Martin Stein
 * \date    2012-04-27
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
#include <cpu.h>
#include <pic.h>
#include <cortex_a9_wugen.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>

using namespace Genode;

Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::RAM_0_BASE, Board::RAM_0_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::MMIO_0_BASE, Board::MMIO_0_SIZE },
		{ Board::MMIO_1_BASE, Board::MMIO_1_SIZE },
		{ Board::DSS_MMIO_BASE, Board::DSS_MMIO_SIZE },
		{ Board::DISPC_MMIO_BASE, Board::DISPC_MMIO_SIZE },
		{ Board::HDMI_MMIO_BASE, Board::HDMI_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* core timer and PIC */
		{ Board::CORTEX_A9_PRIVATE_MEM_BASE,
		  Board::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* core UART */
		{ Board::TL16C750_3_MMIO_BASE, Board::TL16C750_MMIO_SIZE },

		/* l2 cache controller */
		{ Board::PL310_MMIO_BASE, Board::PL310_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


void Cortex_a9::Board::wake_up_all_cpus(void * const ip)
{
	Cortex_a9_wugen wugen;
	wugen.init_cpu_1(ip);
	asm volatile("dsb\n"
	             "sev\n");
}

Genode::Arm::User_context::User_context() { cpsr = Psr::init_user(); }


void Cpu::Actlr::enable_smp() {
	Kernel::board().monitor().call(Board::Secure_monitor::CPU_ACTLR_SMP_BIT_RAISE, 0); }


bool Cortex_a9::Board::errata(Cortex_a9::Board::Errata err)
{
	switch (err) {
		case Cortex_a9::Board::PL310_588369:
		case Cortex_a9::Board::PL310_727915: return true;
		default: ;
	};
	return false;
}
