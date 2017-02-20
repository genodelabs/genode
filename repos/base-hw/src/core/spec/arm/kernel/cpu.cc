/*
 * \brief   Kernel cpu driver implementations specific to ARM
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/pd.h>
#include <kernel/perf_counter.h>
#include <pic.h>
#include <trustzone.h>

void Kernel::Cpu::init(Kernel::Pic &pic)
{
	/* locally initialize interrupt controller */
	pic.init_cpu_local();

	/* TrustZone initialization code */
	init_trustzone(pic);

	/* enable performance counter */
	perf_counter()->enable();

	/* enable timer interrupt */
	pic.unmask(Timer::interrupt_id(id()), id());
}


void Kernel::Cpu_domain_update::_domain_update() {
	cpu_pool()->cpu(Cpu::executing_id())->invalidate_tlb_by_pid(_domain_id); }
