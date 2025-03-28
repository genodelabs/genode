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

/* core includes */
#include <platform.h>
#include <platform_thread.h>

/* base-internal includes */
#include <base/internal/stack.h>

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
	return with_native_thread(
		[&] (Native_thread &nt) {
			nt.pt = new (platform().core_mem_alloc())
				Platform_thread(platform_specific().core_pd(), _stack->name().string());

			nt.pt->pager(platform_specific().core_pager());
			nt.l4id = nt.pt->native_thread_id();

			nt.pt->start((void *)_thread_start, stack_top());
			return Start_result::OK;
		},
		[&] { return Start_result::DENIED; });
}


void Thread::_deinit_native_thread(Stack &stack)
{
	destroy(platform().core_mem_alloc(), stack.native_thread().pt);
}
