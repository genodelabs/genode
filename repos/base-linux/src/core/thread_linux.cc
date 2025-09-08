/*
 * \brief  Implementation of the core-internal Thread API via Linux threads
 * \author Norman Feske
 * \date   2006-06-13
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
#include <base/internal/native_thread.h>
#include <base/internal/stack.h>

/* Linux syscall bindings */
#include <linux_syscalls.h>

using namespace Genode;


static void empty_signal_handler(int) { }


void Thread::_thread_start()
{
	Thread &thread = *Thread::myself();

	thread._stack.with_result(
		[&] (Stack &stack) {

			/* use primary stack as alternate stack for fatal signals (exceptions) */
			void   *stack_base = (void *)stack.base();
			size_t  stack_size = stack.top() - stack.base();

			lx_sigaltstack(stack_base, stack_size);
		},
		[&] (Stack_error) {
			warning("attempt to start thread ", thread.name, " without stack"); }
	);

	/*
	 * Set signal handler such that canceled system calls get not transparently
	 * retried after a signal gets received.
	 */
	lx_sigaction(LX_SIGUSR1, empty_signal_handler, false);

	/*
	 * Deliver SIGCHLD signals to no thread other than the main thread. Core's
	 * main thread will handle the signals while executing the 'wait_for_exit'
	 * function, which is known to not hold any locks that would interfere with
	 * the handling of the signal.
	 */
	lx_sigsetmask(LX_SIGCHLD, false);

	thread.entry();
	thread._join.wakeup();
	sleep_forever();
}


void Thread::_init_native_thread(Stack &, Type) { }


void Thread::_deinit_native_thread(Stack &) { }


Thread::Start_result Thread::start()
{
	return _stack.convert<Start_result>(
		[&] (Stack &stack) {
			Native_thread &nt = stack.native_thread();
			nt.tid = lx_create_thread(Thread::_thread_start, (void *)stack.top());
			nt.pid = lx_getpid();
			return Start_result::OK;
		},
		[&] (Stack_error) { return Start_result::DENIED; });
}
