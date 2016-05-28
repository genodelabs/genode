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

/* base-internal includes */
#include <base/internal/native_thread.h>

/* Linux syscall bindings */
#include <linux_syscalls.h>

using namespace Genode;


static void empty_signal_handler(int) { }

static char signal_stack[0x2000] __attribute__((aligned(0x1000)));

void Thread::_thread_start()
{
	lx_sigaltstack(signal_stack, sizeof(signal_stack));

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

	Thread::myself()->entry();
	Thread::myself()->_join_lock.unlock();
	sleep_forever();
}


void Thread::_init_platform_thread(size_t, Type) { }


void Thread::_deinit_platform_thread() { }


void Thread::start()
{
	native_thread().tid = lx_create_thread(Thread::_thread_start, stack_top(), this);
	native_thread().pid = lx_getpid();
}


void Thread::cancel_blocking() { }
