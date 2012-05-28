/*
 * \brief  NOVA-specific implementation of the Thread API
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
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
	Genode::sleep_forever();
}


/*****************
 ** Thread base **
 *****************/

void Thread_base::_init_platform_thread() { }


void Thread_base::_deinit_platform_thread()
{
	env()->cpu_session()->kill_thread(_thread_cap);
}


void Thread_base::start()
{
	/* create thread at core */
	char buf[48];
	name(buf, sizeof(buf));
	Cpu_session * cpu = env()->cpu_session();
	_thread_cap = cpu->create_thread(buf, (addr_t)&_context->utcb);

	/* assign thread to protection domain */
	env()->pd_session()->bind_thread(_thread_cap);

	/* create new pager object and assign it to the new thread */
	Pager_capability pager_cap = env()->rm_session()->add_client(_thread_cap);
	env()->cpu_session()->set_pager(_thread_cap, pager_cap);

	/* register initial IP and SP at core */
	addr_t thread_sp = (addr_t)&_context->stack[-4];
	thread_sp &= ~0xf;  /* align initial stack to 16 byte boundary */
	env()->cpu_session()->start(_thread_cap, (addr_t)_thread_start, thread_sp);
}


void Thread_base::cancel_blocking()
{
	env()->cpu_session()->cancel_blocking(_thread_cap);
}
