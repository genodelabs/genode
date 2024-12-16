/*
 * \brief  Implementation of Thread API interface on top of Platform_thread
 * \author Norman Feske
 * \date   2006-05-03
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/stack.h>

/* core includes */
#include <platform.h>

using namespace Core;


void Thread::_thread_start()
{
	Thread::myself()->_thread_bootstrap();
	Thread::myself()->entry();
	Thread::myself()->_join.wakeup();
	sleep_forever();
}


Thread::Start_result Thread::start()
{
	/* create and start platform thread */
	native_thread().pt = new (platform().core_mem_alloc())
		Platform_thread(platform_specific().core_pd(), _stack->name().string());

	native_thread().pt->pager(platform_specific().core_pager());
	native_thread().l4id = native_thread().pt->native_thread_id();

	native_thread().pt->start((void *)_thread_start, stack_top());

	return Start_result::OK;
}


void Thread::_deinit_platform_thread()
{
	/* destruct platform thread */
	destroy(platform().core_mem_alloc(), native_thread().pt);
}
