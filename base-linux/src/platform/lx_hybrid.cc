/*
 * \brief  Supplemental code for hybrid Genode/Linux programs
 * \author Norman Feske
 * \date   2011-09-02
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/crt0.h>
#include <base/printf.h>
#include <_main_helper.h>


extern "C" int raw_write_str(const char *str);

enum { verbose_atexit = false };


/**
 * Dummy for symbol that is normally provided by '_main.cc'
 */
int genode___cxa_atexit(void (*func)(void*), void *arg, void *dso)
{
	if (verbose_atexit)
		raw_write_str("genode___cxa_atexit called, not implemented\n");
	return 0;
}


/*
 * Manually initialize the 'lx_environ' pointer. For non-hybrid programs, this
 * pointer is initialized by the startup code.
 */
extern char **environ;
extern char **lx_environ;

/*
 * This function must be called before any other static constructor in the Genode
 * application, so it gets the highest priority (lowest priority number >100)
 */
__attribute__((constructor(101))) void lx_hybrid_init()
{
	main_thread_bootstrap();
	lx_environ = environ;
}


/************
 ** Thread **
 ************/

/*
 * For hybrid Linux/Genode programs, Genode's thread API is implemented via
 * POSIX threads.
 *
 * Hybrid Linux/Genode programs are linked against the glibc along with other
 * native Linux libraries. Such libraries may use the 'pthread' API to spawn
 * threads, which then may call Genode code. Vice versa, Genode threads may
 * interact with code of a native Linux libraries. Hence, both worlds Genode
 * and native Linux libraries should use the same underlying threading API.
 * Furthermore, using the pthread API is a precondition to satisfy the glibc's
 * assumption about thread-local storage, which is particularly important
 * for the correct thread-local handling of 'errno'. As another benefit of
 * using the pthread API over the normal Genode thread implementation, hybrid
 * Linux/Genode programs comply with the GNU debugger's expectations. Such
 * programs can be debugged as normal Linux programs.
 *
 * Genode's normal thread API for Linux was introduced to decouple Genode
 * from the glibc. This is especially important when using Genode's libc
 * Mixing both Genode's libc and glibc won't work.
 */

/* Genode includes */
#include <base/thread.h>

/* libc includes */
#include <pthread.h>
#include <stdio.h>
#include <errno.h>

using namespace Genode;


/**
 * Return TLS key used to storing the thread meta data
 */
static pthread_key_t tls_key()
{
	struct Tls_key
	{
		pthread_key_t key;

		Tls_key()
		{
			pthread_key_create(&key, 0);
		}
	};

	static Tls_key inst;
	return inst.key;
}


namespace Genode {

	struct Thread_meta_data
	{
		/**
		 * Used to block the constructor until the new thread has initialized
		 * 'id'
		 */
		Lock construct_lock;

		/**
		 * Used to block the new thread until 'start' is called
		 */
		Lock start_lock;

		/**
		 * Filled out by 'thread_start' function in the context of the new
		 * thread
		 */
		Thread_base * const thread_base;

		/**
		 * POSIX thread handle
		 */
		pthread_t pt;

		/**
		 * Constructor
		 *
		 * \param thread_base  associated 'Thread_base' object
		 */
		Thread_meta_data(Thread_base *thread_base)
		:
			construct_lock(Lock::LOCKED), start_lock(Lock::LOCKED),
			thread_base(thread_base)
		{ }
	};
}


static void empty_signal_handler(int) { }


static void *thread_start(void *arg)
{
	/*
	 * Set signal handler such that canceled system calls get not
	 * transparently retried after a signal gets received.
	 */
	lx_sigaction(LX_SIGUSR1, empty_signal_handler);

	/* assign 'Thread_meta_data' pointer to TLS entry */
	pthread_setspecific(tls_key(), arg);

	/* enable immediate cancellation when calling 'pthread_cancel' */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	/*
	 * Initialize thread meta data
	 */
	Thread_meta_data *meta_data = (Thread_meta_data *)arg;
	meta_data->thread_base->tid().tid = lx_gettid();
	meta_data->thread_base->tid().pid = lx_getpid();

	/* unblock 'Thread_base' constructor */
	meta_data->construct_lock.unlock();

	/* block until the 'Thread_base::start' gets called */
	meta_data->start_lock.lock();

	Thread_base::myself()->entry();
	return 0;
}


Thread_base *Thread_base::myself()
{
	void *tls = pthread_getspecific(tls_key());

	bool const is_main_thread = (tls == 0);

	return is_main_thread ? 0 : ((Thread_meta_data *)tls)->thread_base;
}


void Thread_base::start()
{
	/*
	 * Unblock thread that is supposed to slumber in 'thread_start'.
	 */
	_tid.meta_data->start_lock.unlock();
}


Thread_base::Thread_base(const char *name, size_t stack_size)
: _list_element(this)
{
	_tid.meta_data = new Thread_meta_data(this);

	int const ret = pthread_create(&_tid.meta_data->pt, 0, thread_start,
	                              _tid.meta_data);
	if (ret) {
		PERR("pthread_create failed (returned %d, errno=%d)",
		     ret, errno);
		delete _tid.meta_data;
		throw Context_alloc_failed();
	}

	_tid.meta_data->construct_lock.lock();
}


void Thread_base::cancel_blocking()
{
	/*
	 * XXX implement interaction with CPU session
	 */
}


Thread_base::~Thread_base()
{
	{
		int const ret = pthread_cancel(_tid.meta_data->pt);
		if (ret)
			PWRN("pthread_cancel unexpectedly returned with %d", ret);
	}

	{
		int const ret = pthread_join(_tid.meta_data->pt, 0);
		if (ret)
			PWRN("pthread_join unexpectedly returned with %d (errno=%d)",
			     ret, errno);
	}

	delete _tid.meta_data;
	_tid.meta_data = 0;
}
