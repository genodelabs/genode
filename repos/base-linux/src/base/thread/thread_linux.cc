/*
 * \brief  Implementation of the Thread API via Linux threads
 * \author Norman Feske
 * \author Martin Stein
 * \date   2006-06-13
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/thread.h>
#include <base/snprintf.h>
#include <base/sleep.h>
#include <linux_cpu_session/linux_cpu_session.h>

/* Linux syscall bindings */
#include <linux_syscalls.h>

using namespace Genode;

extern int main_thread_futex_counter;

static void empty_signal_handler(int) { }


static Lock &startup_lock()
{
	static Lock lock(Lock::LOCKED);
	return lock;
}


/**
 * Signal handler for killing the thread
 */
static void thread_exit_signal_handler(int) { lx_exit(0); }


void Thread_base::_thread_start()
{
	/*
	 * Set signal handler such that canceled system calls get not
	 * transparently retried after a signal gets received.
	 */
	lx_sigaction(LX_SIGUSR1, empty_signal_handler);

	Thread_base * const thread = Thread_base::myself();

	/* inform core about the new thread and process ID of the new thread */
	Linux_cpu_session *cpu = dynamic_cast<Linux_cpu_session *>(thread->_cpu_session);
	if (cpu)
		cpu->thread_id(thread->cap(), thread->tid().pid, thread->tid().tid);

	/* wakeup 'start' function */
	startup_lock().unlock();

	thread->entry();

	/* unblock caller of 'join()' */
	thread->_join_lock.unlock();

	sleep_forever();
}


void Thread_base::_init_platform_thread(size_t weight, Type type)
{
	/* if no cpu session is given, use it from the environment */
	if (!_cpu_session)
		_cpu_session = env()->cpu_session();

	/* for normal threads create an object at the CPU session */
	if (type == NORMAL) {
		_thread_cap = _cpu_session->create_thread(weight, _context->name);
		return;
	}
	/* adjust initial object state for main threads */
	tid().futex_counter = main_thread_futex_counter;
	_thread_cap = env()->parent()->main_thread_cap();
}


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
	_cpu_session->kill_thread(_thread_cap);
}


void Thread_base::start()
{
	/* synchronize calls of the 'start' function */
	static Lock lock;
	Lock::Guard guard(lock);

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

	_tid.tid = lx_create_thread(Thread_base::_thread_start, stack_top(), this);
	_tid.pid = lx_getpid();

	/* wait until the 'thread_start' function got entered */
	startup_lock().lock();
}


void Thread_base::cancel_blocking()
{
	_cpu_session->cancel_blocking(_thread_cap);
}
