/*
 * \brief  Implementation of the Thread API via Linux threads
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
#include <base/env.h>
#include <base/thread.h>
#include <base/snprintf.h>
#include <base/sleep.h>

/* Linux syscall bindings */
#include <linux_syscalls.h>

using namespace Genode;


static void empty_signal_handler(int) { }


/**
 * Signal handler for killing the thread
 */
static void thread_exit_signal_handler(int) { lx_exit(0); }


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


void Thread_base::_deinit_platform_thread()
{
	/*
	 * Kill thread until it is really really dead
	 *
	 * We use the 'tgkill' system call to kill the thread. This system call
	 * returns immediately and just flags the corresponding signal at the
	 * targeted thread context. However, the thread still lives until the
	 * signal flags are evaluated. When leaving this function, however, we want
	 * to be sure that the thread is no more executing any code such that we
	 * an safely free and unmap the thread's stack. So we call 'tgkill' in a
	 * loop until we get an error indicating that the thread does not exists
	 * anymore.
	 */
	for (;;) {

		/* destroy thread locally */
		int ret = lx_tgkill(_tid.pid, _tid.tid, LX_SIGCANCEL);

		if (ret < 0) break;

		/* if thread still exists, wait a bit and try to kill it again */
		struct timespec ts = { 0, 500 };
		lx_nanosleep(&ts, 0);
	}

	/* inform core about the killed thread */
	env()->cpu_session()->kill_thread(_thread_cap);
}


void Thread_base::start()
{
	/*
	 * The first time we enter this code path, the 'start' function is
	 * called by the main thread as there cannot exist other threads
	 * without executing this function. When first called, we initialize
	 * the thread lib here.
	 */
	static bool threadlib_initialized = false;
	if (!threadlib_initialized) {
		lx_sigaction(LX_SIGCANCEL, thread_exit_signal_handler);
		threadlib_initialized = true;
	}

	/* align initial stack to 16 byte boundary */
	void *thread_sp = (void *)((addr_t)(_context->stack) & ~0xf);
	_tid.tid = lx_create_thread(thread_start, thread_sp, this);
	_tid.pid = lx_getpid();

	/*
	 * Inform core about the new thread by calling create_thread and encoding
	 * the thread's PID in the thread-name argument.
	 */
	char name_and_pid[Cpu_session::THREAD_NAME_LEN + 2*16];
	snprintf(name_and_pid, sizeof(name_and_pid), "%s:0x%x:0x%x",
	         _context->name, _tid.tid, _tid.pid);
	_thread_cap = env()->cpu_session()->create_thread(name_and_pid);
}


void Thread_base::cancel_blocking()
{
	env()->cpu_session()->cancel_blocking(_thread_cap);
}
