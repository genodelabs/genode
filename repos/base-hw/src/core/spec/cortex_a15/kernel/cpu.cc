/*
 * \brief   Cpu class implementation specific to Cortex A15 SMP
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
#include <kernel/lock.h>
#include <kernel/pd.h>
#include <pic.h>
#include <board.h>

/* base-hw includes */
#include <kernel/perf_counter.h>

using namespace Kernel;

/* entrypoint for non-boot CPUs */
extern "C" void * _start_secondary_cpus;

/* indicates boot cpu status */
static volatile bool primary_cpu = true;


void Kernel::Cpu::init(Kernel::Pic &pic/*, Kernel::Pd & core_pd, Genode::Board & board*/)
{
	{
		Lock::Guard guard(data_lock());

		/* enable performance counter */
		perf_counter()->enable();

		/* enable timer interrupt */
		pic.unmask(Timer::interrupt_id(id()), id());
	}
}
