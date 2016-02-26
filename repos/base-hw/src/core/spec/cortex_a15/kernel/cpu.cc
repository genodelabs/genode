/*
 * \brief   Cpu class implementation specific to Cortex A15 SMP
 * \author  Stefan Kalkowski
 * \date    2015-12-09
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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


void Kernel::Cpu::init(Kernel::Pic &pic, Kernel::Pd & core_pd, Genode::Board & board)
{
	/*
	 * local interrupt controller interface needs to be initialized that early,
	 * because it potentially sets the SGI interrupts to be non-secure before
	 * entering the normal world in Genode::Cpu::init()
	 */
	pic.init_cpu_local();

	Genode::Cpu::init();

	Sctlr::init();
	Psr::write(Psr::init_kernel());

	invalidate_inner_data_cache();

	/* primary cpu wakes up all others */
	if (primary_cpu && NR_OF_CPUS > 1) {
		primary_cpu = false;
		board.wake_up_all_cpus(&_start_secondary_cpus);
	}

	enable_mmu_and_caches(core_pd);

	{
		Lock::Guard guard(data_lock());

		/* enable performance counter */
		perf_counter()->enable();

		/* enable timer interrupt */
		pic.unmask(Timer::interrupt_id(id()), id());
	}
}
