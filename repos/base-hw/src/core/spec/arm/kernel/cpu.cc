/*
 * \brief   Kernel cpu driver implementations specific to ARM
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/pd.h>
#include <kernel/perf_counter.h>
#include <pic.h>
#include <trustzone.h>

void Kernel::Cpu::init(Kernel::Pic &pic, Kernel::Pd & core_pd,
                       Genode::Board & board)
{
	Sctlr::init();

	/* switch to core address space */
	Cpu::enable_mmu_and_caches(core_pd);

	/*
	 * TrustZone initialization code
	 */
	init_trustzone(pic);

	/*
	 * Enable performance counter
	 */
	perf_counter()->enable();

	/* locally initialize interrupt controller */
	pic.init_cpu_local();

	/* enable timer interrupt */
	pic.unmask(Timer::interrupt_id(id()), id());
}


void Kernel::Cpu_domain_update::_domain_update() {
	cpu_pool()->cpu(Cpu::executing_id())->invalidate_tlb_by_pid(_domain_id); }
