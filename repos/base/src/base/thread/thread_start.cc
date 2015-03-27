/*
 * \brief  NOVA-specific implementation of the Thread API
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/env.h>

using namespace Genode;


/**
 * Entry point entered by new threads
 */
void Thread_base::_thread_start()
{
	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	Thread_base::myself()->_join_lock.unlock();
	Genode::sleep_forever();
}


/*****************
 ** Thread base **
 *****************/

void Thread_base::_deinit_platform_thread()
{
	if (!_cpu_session)
		_cpu_session = env()->cpu_session();

	_cpu_session->kill_thread(_thread_cap);
}


void Thread_base::start()
{
	/* if no cpu session is given, use it from the environment */
	if (!_cpu_session)
		_cpu_session = env()->cpu_session();

	/* create thread at core */
	char buf[48];
	name(buf, sizeof(buf));
	enum { WEIGHT = Cpu_session::DEFAULT_WEIGHT };
	addr_t const utcb = (addr_t)&_context->utcb;
	_thread_cap = _cpu_session->create_thread(WEIGHT, buf, utcb);
	if (!_thread_cap.valid())
		throw Cpu_session::Thread_creation_failed();

	/* assign thread to protection domain */
	if (env()->pd_session()->bind_thread(_thread_cap))
		throw Cpu_session::Thread_creation_failed();

	/* create new pager object and assign it to the new thread */
	Pager_capability pager_cap = env()->rm_session()->add_client(_thread_cap);
	if (!pager_cap.valid())
		throw Cpu_session::Thread_creation_failed();

	_cpu_session->set_pager(_thread_cap, pager_cap);

	/* register initial IP and SP at core */
	_cpu_session->start(_thread_cap, (addr_t)_thread_start, _context->stack_top());
}


void Thread_base::cancel_blocking()
{
	_cpu_session->cancel_blocking(_thread_cap);
}
