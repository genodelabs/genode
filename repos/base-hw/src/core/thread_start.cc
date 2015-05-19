/*
 * \brief  Implementation of Thread API interface for core
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-02-27
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>
#include <base/env.h>

/* core includes */
#include <map_local.h>
#include <kernel/kernel.h>
#include <platform.h>
#include <platform_thread.h>

using namespace Genode;

namespace Genode { Rm_session * env_context_area_rm_session(); }

namespace Hw {
	extern Untyped_capability _main_thread_cap;
}

void Thread_base::start()
{
	/* start thread with stack pointer at the top of stack */
	if (_tid.platform_thread->start((void *)&_thread_start, stack_top()))
		PERR("failed to start thread");
}


void Thread_base::cancel_blocking()
{
	_tid.platform_thread->cancel_blocking();
}


void Thread_base::_deinit_platform_thread()
{
	/* destruct platform thread */
	destroy(platform()->core_mem_alloc(), _tid.platform_thread);
}


void Thread_base::_init_platform_thread(size_t, Type type)
{
	if (type == NORMAL) {
		_tid.platform_thread = new (platform()->core_mem_alloc())
			Platform_thread(_context->name, &_context->utcb);
		return;
	}

	/* remap initial main-thread UTCB according to context-area spec */
	Genode::map_local((addr_t)Kernel::Core_thread::singleton().utcb(),
	                  (addr_t)&_context->utcb,
	                  max(sizeof(Native_utcb) / get_page_size(), (size_t)1));

	/* adjust initial object state in case of a main thread */
	tid().cap = Hw::_main_thread_cap.dst();
}
