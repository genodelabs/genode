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

void Kernel::Cpu::init(Kernel::Pic &pic)
{
	/* enable performance counter */
	perf_counter()->enable();

	/* enable timer interrupt */
	pic.unmask(_timer.interrupt_id(), id());
}


void Kernel::Cpu_domain_update::_domain_update()
{
	/* flush TLB by ASID */
	if (_domain_id)
		Cpu::Tlbiasid::write(_domain_id);
	else
		Cpu::Tlbiall::write(0);
}
