/*
 * \brief  Implementation of Thread API interface on top of Platform_thread
 * \author Norman Feske
 * \date   2006-05-03
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>

/* core includes */
#include <platform.h>
#include <core_env.h>

using namespace Genode;


void Thread_base::_thread_start()
{
	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	Thread_base::myself()->_join_lock.unlock();
	sleep_forever();
}


void Thread_base::start()
{
	/* create and start platform thread */
	_tid.pt = new(platform()->core_mem_alloc()) Platform_thread(_context->name);

	platform_specific()->core_pd()->bind_thread(_tid.pt);

	_tid.pt->pager(platform_specific()->core_pager());
	_tid.l4id = _tid.pt->native_thread_id();

	_tid.pt->start((void *)_thread_start, stack_top());
}


void Thread_base::cancel_blocking()
{
	/*
	 * Within core, we never need to unblock threads
	 */
}


void Thread_base::_init_platform_thread() { }


void Thread_base::_deinit_platform_thread()
{
	/* destruct platform thread */
	destroy(platform()->core_mem_alloc(), _tid.pt);
}
