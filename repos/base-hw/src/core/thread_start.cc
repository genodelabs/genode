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
#include <platform.h>
#include <platform_thread.h>

using namespace Genode;

namespace Genode { Rm_session * env_context_area_rm_session(); }

extern Ram_dataspace_capability _main_thread_utcb_ds;
extern Native_thread_id         _main_thread_id;

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

	size_t const utcb_size = sizeof(Native_utcb);
	addr_t const context_area = Native_config::context_area_virtual_base();
	addr_t const utcb_new = (addr_t)&_context->utcb - context_area;
	Rm_session * const rm = env_context_area_rm_session();

	/* remap initial main-thread UTCB according to context-area spec */
	try { rm->attach_at(_main_thread_utcb_ds, utcb_new, utcb_size); }
	catch(...) {
		PERR("failed to re-map UTCB");
		while (1) ;
	}
	/* adjust initial object state in case of a main thread */
	tid().thread_id = _main_thread_id;
}
