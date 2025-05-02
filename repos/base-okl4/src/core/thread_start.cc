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

using namespace Genode;


void Thread::_thread_start()
{
	Thread::myself()->_thread_bootstrap();
	Thread::myself()->entry();
	Thread::myself()->_join.wakeup();
	sleep_forever();
}


Thread::Start_result Thread::start()
{
	return _stack.convert<Start_result>([&] (Stack &stack) {

		Native_thread &nt = stack.native_thread();

		nt.pt = new (Core::platform_specific().thread_slab())
			Core::Platform_thread(Core::platform_specific().core_pd(),
			                      name.string());

		nt.pt->start((void *)_thread_start, (void *)stack.top());

		return Start_result::OK;

	}, [&] (Stack_error) { return Start_result::DENIED; });
}


void Thread::_deinit_native_thread(Stack &stack)
{
	/* destruct platform thread */
	destroy(Core::platform_specific().thread_slab(), stack.native_thread().pt);
}
