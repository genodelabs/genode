/*
 * \brief  Implementation of the core-internal Thread API via Linux threads
 * \author Norman Feske
 * \date   2006-06-13
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

/* Linux syscall bindings */
#include <linux_syscalls.h>

using namespace Genode;


static void empty_signal_handler(int) { }


void Thread_base::_thread_start()
{
	/*
	 * Set signal handler such that canceled system calls get not transparently
	 * retried after a signal gets received.
	 */
	lx_sigaction(LX_SIGUSR1, empty_signal_handler);

	/*
	 * Deliver SIGCHLD signals to no thread other than the main thread. Core's
	 * main thread will handle the signals while executing the 'wait_for_exit'
	 * function, which is known to not hold any locks that would interfere with
	 * the handling of the signal.
	 */
	lx_sigsetmask(LX_SIGCHLD, false);

	Thread_base::myself()->entry();
	Thread_base::myself()->_join_lock.unlock();
	sleep_forever();
}


void Thread_base::_init_platform_thread(size_t, Type) { }


void Thread_base::_deinit_platform_thread() { }


void Thread_base::start()
{
	_tid.tid = lx_create_thread(Thread_base::_thread_start, stack_top(), this);
	_tid.pid = lx_getpid();
}


void Thread_base::cancel_blocking() { }
