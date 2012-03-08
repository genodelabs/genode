/*
 * \brief  Fiasco-specific implementation of the non-core startup Thread API
 * \author Norman Feske
 * \author Stefan Kalkowski
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


void Thread_base::_deinit_platform_thread()
{
	env()->cpu_session()->kill_thread(_thread_cap);
}


void Thread_base::start()
{
	/* create thread at core */
	char buf[48];
	name(buf, sizeof(buf));
	_thread_cap = env()->cpu_session()->create_thread(buf);

	/* assign thread to protection domain */
	env()->pd_session()->bind_thread(_thread_cap);

	/* create new pager object and assign it to the new thread */
	Pager_capability pager_cap = env()->rm_session()->add_client(_thread_cap);
	env()->cpu_session()->set_pager(_thread_cap, pager_cap);

	/* register initial IP and SP at core */
	addr_t thread_sp = (addr_t)&_context->stack[-4];
	thread_sp &= ~0xf;  /* align initial stack to 16 byte boundary */
	env()->cpu_session()->start(_thread_cap, (addr_t)_thread_start, thread_sp);

	/* get gate-capability and badge of new thread */
	Thread_state state;
	env()->cpu_session()->state(_thread_cap, &state);
	_tid = state.cap.tid();

	/*
	 * send newly constructed thread, pointer to its Thread_base object,
	 * and its badge
	 */
	Msgbuf<128> snd_msg, rcv_msg;
	Ipc_client cli(state.cap, &snd_msg, &rcv_msg);
	cli << (addr_t)this << state.cap.local_name() << IPC_CALL;
}


void Thread_base::cancel_blocking()
{
	env()->cpu_session()->cancel_blocking(_thread_cap);
}
