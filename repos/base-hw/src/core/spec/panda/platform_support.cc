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

Memory_region_array & Platform::ram_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::RAM_0_BASE, Board::RAM_0_SIZE });
}


Memory_region_array & Platform::core_mmio_regions()
{
	return *unmanaged_singleton<Memory_region_array>(
		Memory_region { Board::CORTEX_A9_PRIVATE_MEM_BASE,   /* timer and PIC */
		                Board::CORTEX_A9_PRIVATE_MEM_SIZE },
		Memory_region { Board::TL16C750_3_MMIO_BASE,                  /* UART */
		                Board::TL16C750_MMIO_SIZE },
		Memory_region { Board::PL310_MMIO_BASE,        /* l2 cache controller */
		                Board::PL310_MMIO_SIZE });
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
