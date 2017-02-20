/*
 * \brief   Parts of platform that are specific to Pandaboard
 * \author  Stefan Kalkowski
 * \date    2017-01-30
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <cortex_a9_wugen.h>

Platform::Board::Board()
: early_ram_regions(Memory_region { RAM_0_BASE, RAM_0_SIZE }),
  core_mmio(Memory_region { CORTEX_A9_PRIVATE_MEM_BASE,
                            CORTEX_A9_PRIVATE_MEM_SIZE },
            Memory_region { TL16C750_3_MMIO_BASE,
                            TL16C750_MMIO_SIZE },
            Memory_region { PL310_MMIO_BASE,
                            PL310_MMIO_SIZE }) { }


void Cortex_a9::Board::wake_up_all_cpus(void * const ip)
{
	Genode::Cortex_a9_wugen wugen;
	wugen.init_cpu_1(ip);
	asm volatile("dsb\n"
	             "sev\n");
}


bool Cortex_a9::Board::errata(Cortex_a9::Board::Errata err)
{
	switch (err) {
		case Cortex_a9::Board::PL310_588369:
		case Cortex_a9::Board::PL310_727915: return true;
		default: ;
	};
	return false;
}


void Genode::Cpu::Actlr::enable_smp(Genode::Board & board)
{
	Board::Secure_monitor monitor;
	monitor.call(Board::Secure_monitor::CPU_ACTLR_SMP_BIT_RAISE, 0);
}
