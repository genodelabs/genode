/*
 * \brief  Implementation of the Thread API
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/env.h>
#include <cpu_thread/client.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/globals.h>

using namespace Genode;


static Capability<Pd_session> pd_session_cap(Capability<Pd_session> pd_cap = { })
{
	static Capability<Pd_session> cap = pd_cap; /* defined once by 'init_thread_start' */
	return cap;
}


/**
 * Entry point entered by new threads
 */
void Thread::_thread_start()
{
	Thread::myself()->_thread_bootstrap();

	/* catch any exception at this point and try to print an error message */
	try {
		Thread::myself()->entry();
	} catch (...) {
		try {
			raw("Thread '", Thread::myself()->name,
			    "' died because of an uncaught exception");
		} catch (...) {
			/* die in a noisy way */
			*(unsigned long *)0 = 0xdead;
		}
		throw;
	}

	Thread::myself()->_join.wakeup();

	/* sleep silently */
	Genode::sleep_forever();
}


/************
 ** Thread **
 ************/

void Thread::_deinit_native_thread(Stack &)
{
	if (!_cpu_session) {
		error("Thread::_cpu_session unexpectedly not defined");
		return;
	}

	_thread_cap.with_result(
		[&] (Thread_capability cap) { _cpu_session->kill_thread(cap); },
		[&] (auto) { });
}


Thread::Start_result Thread::start()
{
	_init_cpu_session_and_trace_control();

	return _stack.convert<Start_result>([&] (Stack &stack) {

		/* create thread at core */
		addr_t const utcb = addr_t(&stack.utcb());

		_thread_cap = _cpu_session->create_thread(pd_session_cap(), name, _affinity,
		                                          Weight(), utcb);
		return _thread_cap.convert<Start_result>(
			[&] (Thread_capability cap) {

				/* start execution at initial instruction pointer and stack pointer */
				Cpu_thread_client(cap).start(addr_t(_thread_start), stack.top());
				return Start_result::OK;
			},
			[&] (Cpu_session::Create_thread_error) { return Start_result::DENIED; });

	}, [&] (Stack_error) { return Start_result::DENIED; });
}


void Genode::init_thread_start(Capability<Pd_session> pd_cap)
{
	pd_session_cap(pd_cap);
}
