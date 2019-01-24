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
#include <pic.h>
#include <board.h>
#include <platform_thread.h>

/* base includes */
#include <base/internal/unmanaged_singleton.h>

using namespace Kernel;

static_assert(sizeof(Genode::sizet_arithm_t) >= 2 * sizeof(size_t),
	"Bad result type for size_t arithmetics.");

Pd &Kernel::core_pd() {
	return unmanaged_singleton<Genode::Core_platform_pd>()->kernel_pd(); }


Pic &Kernel::pic() { return *unmanaged_singleton<Pic>(); }

extern "C" void kernel_init();

/**
 * Setup kernel environment
 */
extern "C" void kernel_init()
{
	static volatile bool pool_ready   = false;
	static volatile bool kernel_ready = false;

	{
		Lock::Guard guard(data_lock());

		/* initialize current cpu */
		pool_ready = cpu_pool().initialize(pic());
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
