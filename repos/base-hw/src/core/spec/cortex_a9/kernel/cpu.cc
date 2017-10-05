/*
 * \brief   Cpu class implementation specific to Cortex A9 SMP
 * \author  Stefan Kalkowski
 * \date    2015-12-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/perf_counter.h>
#include <kernel/lock.h>
#include <kernel/cpu.h>
#include <kernel/pd.h>
#include <pic.h>
#include <platform_pd.h>
#include <platform.h>


void Kernel::Cpu::init(Kernel::Pic &pic)
{
	_fpu.init();

	{
		Lock::Guard guard(data_lock());

		/* enable performance counter */
		perf_counter()->enable();

		/* enable timer interrupt */
		pic.unmask(_timer.interrupt_id(), id());
	}
}
