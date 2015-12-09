/*
 * \brief  Common kernel initialization
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-10-20
 */

/*
 * Copyright (C) 2011-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/pd.h>
#include <kernel/kernel.h>
#include <kernel/test.h>
#include <platform_pd.h>
#include <pic.h>
#include <platform_thread.h>

/* base includes */
#include <unmanaged_singleton.h>
#include <base/native_types.h>

using namespace Kernel;

static_assert(sizeof(Genode::sizet_arithm_t) >= 2 * sizeof(size_t),
	"Bad result type for size_t arithmetics.");

Pd * Kernel::core_pd() {
	return unmanaged_singleton<Genode::Core_platform_pd>()->kernel_pd(); }


Pic * Kernel::pic() { return unmanaged_singleton<Pic>(); }


/**
 * Setup kernel environment
 */
extern "C" void init_kernel()
{
	/*
	 * As atomic operations are broken in physical mode on some platforms
	 * we must avoid the use of 'cmpxchg' by now (includes not using any
	 * local static objects.
	 */

	/* calculate in advance as needed later when data writes aren't allowed */
	core_pd();

	/* initialize cpu pool */
	cpu_pool();

	/* initialize PIC */
	pic();

	/* initialize current cpu */
	cpu_pool()->cpu(Cpu::executing_id())->init(*pic(), *core_pd());

	Core_thread::singleton();

	Genode::printf("kernel initialized\n");

	test();

	kernel();
}
