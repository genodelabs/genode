/*
 * \brief   Cpu class implementation specific to Armv7 SMP
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2015-12-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/lock.h>
#include <kernel/pd.h>
#include <kernel/kernel.h>
#include <kernel/test.h>
#include <platform_pd.h>
#include <trustzone.h>
#include <timer.h>
#include <pic.h>
#include <board.h>
#include <platform_thread.h>

/* base includes */
#include <unmanaged_singleton.h>
#include <base/native_types.h>

/* base-hw includes */
#include <kernel/perf_counter.h>

using namespace Kernel;

extern "C" void * _start_secondary_cpus;

Lock & Kernel::data_lock() { return *unmanaged_singleton<Kernel::Lock>(); }


/**
 * Setup kernel enviroment after activating secondary CPUs
 */
extern "C" void init_kernel_mp()
{
	/*
	 * As updates on a cached kernel lock might not be visible to CPUs that
	 * have not enabled caches, we can't synchronize the activation of MMU and
	 * caches. Hence we must avoid write access to kernel data by now.
	 */

	/* synchronize data view of all CPUs */
	Cpu::invalidate_data_caches();
	Cpu::invalidate_instr_caches();
	Cpu::data_synchronization_barrier();

	/* locally initialize interrupt controller */
	pic()->init_cpu_local();

	/* initialize CPU in physical mode */
	Cpu::init_phys_kernel();

	/* switch to core address space */
	Cpu::init_virt_kernel(*core_pd());

	/*
	 * Now it's safe to use 'cmpxchg'
	 */
	{
		Lock::Guard guard(data_lock());

		/*
		 * Now it's save to write to kernel data
		 */

		/* TrustZone initialization code */
		init_trustzone(*pic());

		/* enable performance counter */
		perf_counter()->enable();

		/* enable timer interrupt */
		unsigned const cpu = Cpu::executing_id();
		pic()->unmask(Timer::interrupt_id(cpu), cpu);
		PINF("ok CPU awake");
	}
}


void Kernel::Cpu::init(Kernel::Pic &pic, Kernel::Pd & core_pd)
{
	if (NR_OF_CPUS > 1) {
		Genode::Board::secondary_cpus_ip(&_start_secondary_cpus);
		data_synchronization_barrier();
		asm volatile ("sev\n");
	}

	init_kernel_mp();
}
