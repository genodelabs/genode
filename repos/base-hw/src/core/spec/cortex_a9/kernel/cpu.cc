/*
 * \brief   Cpu class implementation specific to Cortex A9 SMP
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
#include <scu.h>
#include <board.h>

/* base-hw includes */
#include <kernel/perf_counter.h>

using namespace Kernel;

/* entrypoint for non-boot CPUs */
extern "C" void * _start_secondary_cpus;


/**
 * SMP-safe simple counter
 */
class Counter
{
	private:

		Kernel::Lock _lock;
		volatile int _value = 0;

	public:

		void inc()
		{
			Kernel::Lock::Guard guard(_lock);
			Genode::memory_barrier();
			_value++;
		}

		void wait_for(int const v) {
			while (_value < v) ; }
};

static volatile bool primary_cpu = true; /* indicates boot cpu status */
static Counter data_cache_invalidated;   /* counts cpus that invalidated
                                            their data cache */
static Counter data_cache_enabled;       /* counts cpus that enabled
                                            their data cache */
static Counter smp_coherency_enabled;    /* counts cpus that enabled
                                            their SCU */


/*
 * The initialization of Cortex A9 multicore systems implies a sophisticated
 * algorithm in early revisions of this cpu.
 *
 * See ARM's Cortex-A9 MPCore TRM r2p0 in section 5.3.5 for more details
 */
void Kernel::Cpu::init(Kernel::Pic &pic, Kernel::Pd & core_pd, Genode::Board & board)
{
	bool primary = primary_cpu;
	if (primary) primary_cpu = false;

	Sctlr::init();
	Psr::write(Psr::init_kernel());

	/* locally initialize interrupt controller */
	pic.init_cpu_local();

	invalidate_inner_data_cache();
	data_cache_invalidated.inc();

	/* primary cpu wakes up all others */
	if (primary && NR_OF_CPUS > 1)
		board.wake_up_all_cpus(&_start_secondary_cpus);

	/* wait for other cores' data cache invalidation */
	data_cache_invalidated.wait_for(NR_OF_CPUS);

	if (primary) {
		Genode::Scu scu(board);
		scu.invalidate();
		board.l2_cache().invalidate();
		scu.enable();
	}

	/* secondary cpus wait for the primary's cache activation */
	if (!primary) data_cache_enabled.wait_for(1);

	enable_mmu_and_caches(core_pd);
	data_cache_enabled.inc();
	clean_invalidate_inner_data_cache();

	/* wait for other cores' data cache activation */
	data_cache_enabled.wait_for(NR_OF_CPUS);

	if (primary) board.l2_cache().enable();

	/* secondary cpus wait for the primary's coherency activation */
	if (!primary) smp_coherency_enabled.wait_for(1);

	Actlr::enable_smp();
	smp_coherency_enabled.inc();

	/* wait for other cores' coherency activation */
	smp_coherency_enabled.wait_for(NR_OF_CPUS);

	_fpu.init();

	{
		Lock::Guard guard(data_lock());

		/* enable performance counter */
		perf_counter()->enable();

		/* enable timer interrupt */
		pic.unmask(Timer::interrupt_id(id()), id());
	}
}
