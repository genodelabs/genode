/*
 * \brief  Implementation of Thread API interface for core
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-02-27
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>
#include <base/env.h>

/* base-internal stack */
#include <base/internal/stack.h>
#include <base/internal/native_utcb.h>

/* core includes */
#include <map_local.h>
#include <kernel/kernel.h>
#include <platform.h>
#include <platform_thread.h>

using namespace Genode;

namespace Genode { Rm_session *env_stack_area_rm_session(); }

namespace Hw { extern Untyped_capability _main_thread_cap; }


void Thread::start()
{
	/* start thread with stack pointer at the top of stack */
	if (native_thread().platform_thread->start((void *)&_thread_start, stack_top()))
		error("failed to start thread");
}


void Thread::cancel_blocking()
{
	native_thread().platform_thread->cancel_blocking();
}


void Thread::_deinit_platform_thread()
{
	/* destruct platform thread */
	destroy(platform()->core_mem_alloc(), native_thread().platform_thread);
}


void Thread::_init_platform_thread(size_t, Type type)
{
	if (type == NORMAL) {
		native_thread().platform_thread = new (platform()->core_mem_alloc())
			Platform_thread(_stack->name().string(), &_stack->utcb());
		return;
	}

	/* remap initial main-thread UTCB according to stack-area spec */
	Genode::map_local(Platform::core_phys_addr((addr_t)Kernel::Core_thread::singleton().utcb()),
	                  (addr_t)&_stack->utcb(),
	                  max(sizeof(Native_utcb) / get_page_size(), (size_t)1));

	/* adjust initial object state in case of a main thread */
	native_thread().cap = Hw::_main_thread_cap;
}
