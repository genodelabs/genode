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
#include <kernel/kernel.h>
#include <platform_pd.h>
#include <pic.h>
#include <board.h>
#include <platform_thread.h>

/* base includes */
#include <base/internal/unmanaged_singleton.h>

using namespace Kernel;

static_assert(sizeof(Genode::sizet_arithm_t) >= 2 * sizeof(size_t),
	"Bad result type for size_t arithmetics.");

Pd * Kernel::core_pd() {
	return &unmanaged_singleton<Genode::Core_platform_pd>()->kernel_pd(); }


Pic * Kernel::pic() { return unmanaged_singleton<Pic>(); }

extern "C" void _start() __attribute__((section(".text.crt0")));

/**
 * Setup kernel environment
 */
extern "C" void _start()
{
	static volatile bool initialized = false;
	if (Cpu::executing_id()) while (!initialized) ;
	else {
		Genode::log("");
		Genode::log("kernel initialized");
	}

	/* initialize cpu pool */
	cpu_pool();

	/* initialize current cpu */
	cpu_pool()->cpu(Cpu::executing_id())->init(*pic());

	Core_thread::singleton();

	if (!Cpu::executing_id()) initialized = true;

	kernel();
}
