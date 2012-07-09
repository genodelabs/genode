/*
 * \brief  Implementation of the core-internal Thread API via Linux threads
 * \author Norman Feske
 * \date   2006-06-13
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
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


static void thread_start(void *)
{
	/*
	 * Set signal handler such that canceled system calls get not
	 * transparently retried after a signal gets received.
	 */
	lx_sigaction(LX_SIGUSR1, empty_signal_handler);

	/*
	 * Prevent children from becoming zombies. (SIG_IGN = 1)
	 */
	lx_sigaction(LX_SIGCHLD, (void (*)(int))1);

	Thread_base::myself()->entry();
	sleep_forever();
}


void Thread_base::_init_platform_thread() { }


void Thread_base::_deinit_platform_thread() { }


void Thread_base::start()
{
	/* align initial stack to 16 byte boundary */
	void *thread_sp = (void *)((addr_t)(_context->stack) & ~0xf);
	_tid.tid = lx_create_thread(thread_start, thread_sp, this);
	_tid.pid = lx_getpid();
}


void Thread_base::cancel_blocking() { }
