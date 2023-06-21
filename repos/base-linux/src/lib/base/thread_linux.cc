/*
 * \brief  Implementation of the Thread API via Linux threads
 * \author Norman Feske
 * \author Martin Stein
 * \date   2006-06-13
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/thread.h>
#include <base/sleep.h>
#include <base/log.h>
#include <linux_native_cpu/client.h>
#include <cpu_thread/client.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/globals.h>

/* Linux syscall bindings */
#include <linux_syscalls.h>

using namespace Genode;


extern int main_thread_futex_counter;


static void empty_signal_handler(int) { }


static Capability<Pd_session> pd_session_cap(Capability<Pd_session> pd_cap = { })
{
	static Capability<Pd_session> cap = pd_cap; /* defined once by 'init_thread_start' */
	return cap;
}


static Thread_capability main_thread_cap(Thread_capability main_cap = { })
{
	static Thread_capability cap = main_cap; /* defined once by 'init_thread_bootstrap' */
	return cap;
}


static Blockade &startup_lock()
{
	static Blockade blockade;
	return blockade;
}


/**
 * Signal handler for killing the thread
 */
static void thread_exit_signal_handler(int) { lx_exit(0); }


void Thread::_thread_start()
{
	Thread * const thread = Thread::myself();

	/* use primary stack as alternate stack for fatal signals (exceptions) */
	void   *stack_base = (void *)thread->_stack->base();
	size_t  stack_size = thread->_stack->top() - thread->_stack->base();

	lx_sigaltstack(stack_base, stack_size);
	if (stack_size < 0x1000)
		raw("small stack of ", stack_size, " bytes for \"", thread->name(),
		    "\" may break Linux signal handling");

	/*
	 * Set signal handler such that canceled system calls get not
	 * transparently retried after a signal gets received.
	 */
	lx_sigaction(LX_SIGUSR1, empty_signal_handler, false);

	/* inform core about the new thread and process ID of the new thread */
	{
		Linux_native_cpu_client native_cpu(thread->_cpu_session->native_cpu());
		native_cpu.thread_id(thread->cap(), thread->native_thread().pid, thread->native_thread().tid);
	}

	/* wakeup 'start' function */
	startup_lock().wakeup();

	thread->entry();

	/* unblock caller of 'join()' */
	thread->_join.wakeup();

	sleep_forever();
}


void Thread::_init_platform_thread(size_t /* weight */, Type type)
{
	/* if no cpu session is given, use it from the environment */
	if (!_cpu_session) {
		error("Thread::_init_platform_thread: _cpu_session not initialized");
		return;
	}

	/* for normal threads create an object at the CPU session */
	if (type == NORMAL) {
		_thread_cap = _cpu_session->create_thread(pd_session_cap(),
		                                          _stack->name().string(),
		                                          Affinity::Location(),
		                                          Weight());
		return;
	}
	/* adjust initial object state for main threads */
	native_thread().futex_counter = main_thread_futex_counter;
	_thread_cap = main_thread_cap();
}


void Thread::_deinit_platform_thread()
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
		int pid = native_thread().pid;
		if (pid == 0) break;

		int ret = lx_tgkill(pid, native_thread().tid, LX_SIGCANCEL);

		if (ret < 0) break;

		/* if thread still exists, wait a bit and try to kill it again */
		struct timespec ts = { 0, 500 };
		lx_nanosleep(&ts, 0);
	}

	/* inform core about the killed thread */
	_cpu_session->kill_thread(_thread_cap);
}


void Thread::start()
{
	/* synchronize calls of the 'start' function */
	static Mutex mutex;
	Mutex::Guard guard(mutex);

	_init_cpu_session_and_trace_control();

	/*
	 * The first time we enter this code path, the 'start' function is
	 * called by the main thread as there cannot exist other threads
	 * without executing this function. When first called, we initialize
	 * the thread lib here.
	 */
	static bool threadlib_initialized = false;
	if (!threadlib_initialized) {
		lx_sigaction(LX_SIGCANCEL, thread_exit_signal_handler, false);
		threadlib_initialized = true;
	}

	native_thread().tid = lx_create_thread(Thread::_thread_start, stack_top());
	native_thread().pid = lx_getpid();

	/* wait until the 'thread_start' function got entered */
	startup_lock().block();
}


void Genode::init_thread_start(Capability<Pd_session> pd_cap)
{
	pd_session_cap(pd_cap);
}


void Genode::init_thread_bootstrap(Cpu_session &cpu, Thread_capability main_cap)
{
	main_thread_cap(main_cap);

	/* register TID and PID of the main thread at core */
	Linux_native_cpu_client native_cpu(cpu.native_cpu());
	native_cpu.thread_id(main_cap, lx_getpid(), lx_gettid());
}
