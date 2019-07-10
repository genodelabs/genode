/*
 * \brief  Common kernel initialization
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2015-12-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/pd.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/lock.h>
#include <platform_pd.h>
#include <board.h>
#include <platform_thread.h>

/* base includes */
#include <base/internal/unmanaged_singleton.h>

using namespace Kernel;

static_assert(sizeof(Genode::sizet_arithm_t) >= 2 * sizeof(size_t),
	"Bad result type for size_t arithmetics.");

Pd &Kernel::core_pd() {
	return unmanaged_singleton<Genode::Core_platform_pd>()->kernel_pd(); }


extern "C" void kernel_init();

/**
 * Setup kernel environment
 */
extern "C" void kernel_init()
{
	static volatile bool lock_ready   = false;
	static volatile bool pool_ready   = false;
	static volatile bool kernel_ready = false;

	/**
	 * It is essential to guard the initialization of the data_lock object
	 * in the SMP case, because otherwise the __cxa_guard_aquire of the cxx
	 * library contention path might get called, which ends up in
	 * calling a Semaphore, which will call Kernel::stop_thread() or
	 * Kernel::yield() system-calls in this code
	 */
	while (Cpu::executing_id() != Cpu::primary_id() && !lock_ready) { ; }

	{
		Lock::Guard guard(data_lock());

		lock_ready = true;

		/* initialize current cpu */
		pool_ready = cpu_pool().initialize();
	};

	/* wait until all cpus have initialized their corresponding cpu object */
	while (!pool_ready) { ; }

	/* the boot-cpu initializes the rest of the kernel */
	if (Cpu::executing_id() == Cpu::primary_id()) {
		Lock::Guard guard(data_lock());

		Genode::log("");
		Genode::log("kernel initialized");

		Core_thread::singleton();
		kernel_ready = true;
	} else {
		/* secondary cpus spin until the kernel is initialized */
		while (!kernel_ready) {;}
	}

	kernel();
}
