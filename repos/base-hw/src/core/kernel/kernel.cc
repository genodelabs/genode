/*
 * \brief  Singlethreaded minimalistic kernel
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-10-20
 *
 * This kernel is the only code except the mode transition PIC, that runs in
 * privileged CPU mode. It has two tasks. First it initializes the process
 * 'core', enriches it with the whole identically mapped address range,
 * joins and applies it, assigns one thread to it with a userdefined
 * entrypoint (the core main thread) and starts this thread in userland.
 * Afterwards it is called each time an exception occurs in userland to do
 * a minimum of appropriate exception handling. Thus it holds a CPU context
 * for itself as for any other thread. But due to the fact that it never
 * relies on prior kernel runs this context only holds some constant pointers
 * such as SP and IP.
 */

/*
 * Copyright (C) 2011-2015 Genode Labs GmbH
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
#include <platform_thread.h>

/* base includes */
#include <unmanaged_singleton.h>
#include <base/native_types.h>

/* base-hw includes */
#include <kernel/perf_counter.h>

using namespace Kernel;

extern void * _start_secondary_cpus;

static_assert(sizeof(Genode::sizet_arithm_t) >= 2 * sizeof(size_t),
	"Bad result type for size_t arithmetics.");

Lock & Kernel::data_lock() { return *unmanaged_singleton<Kernel::Lock>(); }


Pd * Kernel::core_pd() {
	return unmanaged_singleton<Genode::Core_platform_pd>()->kernel_pd(); }


Pic * Kernel::pic() { return unmanaged_singleton<Pic>(); }


/**
 * Setup kernel environment before activating secondary CPUs
 */
extern "C" void init_kernel_up()
{
	/*
	 * As atomic operations are broken in physical mode on some platforms
	 * we must avoid the use of 'cmpxchg' by now (includes not using any
	 * local static objects.
	 */

	/* calculate in advance as needed later when data writes aren't allowed */
	core_pd();

	/* initialize all CPU objects */
	cpu_pool();

	/* initialize PIC */
	pic();

	/* go multiprocessor mode */
	Cpu::start_secondary_cpus(&_start_secondary_cpus);
}


/**
 * Setup kernel enviroment after activating secondary CPUs as primary CPU
 */
void init_kernel_mp_primary()
{
	Core_thread::singleton();
	Genode::printf("kernel initialized\n");
	test();
}


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
	Cpu::init_virt_kernel(core_pd());

	/*
	 * Now it's safe to use 'cmpxchg'
	 */

	Lock::Guard guard(data_lock());

	/*
	 * Now it's save to write to kernel data
	 */

	/*
	 * TrustZone initialization code
	 *
	 * FIXME This is a plattform specific feature
	 */
	init_trustzone(pic());

	/*
	 * Enable performance counter
	 *
	 * FIXME This is an optional CPU specific feature
	 */
	perf_counter()->enable();

	/* enable timer interrupt */
	unsigned const cpu = Cpu::executing_id();
	pic()->unmask(Timer::interrupt_id(cpu), cpu);

	/* do further initialization only as primary CPU */
	if (Cpu::primary_id() != cpu) { return; }
	init_kernel_mp_primary();
}


extern "C" void kernel()
{
	data_lock().lock();
	cpu_pool()->cpu(Cpu::executing_id())->exception();
}
