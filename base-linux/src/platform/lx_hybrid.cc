/*
 * \brief  Supplemental code for hybrid Genode/Linux programs
 * \author Norman Feske
 * \date   2011-09-02
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
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
	lx_environ = environ;
}

/*
 * Dummy symbols to let generic tests programs (i.e., 'test-config_args') link
 * successfully. Please note that such programs are not expected to work when
 * built as hybrid Linux/Genode programs because when using the glibc startup
 * code, we cannot manipulate argv prior executing main. However, by defining
 * these symbols, we prevent the automated build bot from stumbling over such
 * binaries.
 */
char **genode_argv = 0;
int    genode_argc = 1;

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
#include <base/env.h>

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


static void adopt_thread(Thread_meta_data *meta_data)
{
	/*
	 * Set signal handler such that canceled system calls get not
	 * transparently retried after a signal gets received.
	 */
	lx_sigaction(LX_SIGUSR1, empty_signal_handler);

	/* assign 'Thread_meta_data' pointer to TLS entry */
	pthread_setspecific(tls_key(), meta_data);

	/* enable immediate cancellation when calling 'pthread_cancel' */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	/*
	 * Initialize thread meta data
	 */
	Native_thread &native_thread = meta_data->thread_base->tid();
	native_thread.tid = lx_gettid();
	native_thread.pid = lx_getpid();
}


static void *thread_start(void *arg)
{
	Thread_meta_data *meta_data = (Thread_meta_data *)arg;

	adopt_thread(meta_data);

	/* unblock 'Thread_base' constructor */
	meta_data->construct_lock.unlock();

	/* block until the 'Thread_base::start' gets called */
	meta_data->start_lock.lock();

	Thread_base::myself()->entry();
	return 0;
}


extern "C" void *malloc(::size_t size);


Thread_base *Thread_base::myself()
{
	void * const tls = pthread_getspecific(tls_key());

	if (tls != 0)
		return ((Thread_meta_data *)tls)->thread_base;

	bool const is_main_thread = (lx_getpid() == lx_gettid());
	if (is_main_thread)
		return 0;

	/*
	 * The function was called from a thread created by other means than
	 * Genode's thread API. This may happen if a native Linux library creates
	 * threads via the pthread library. If such a thread calls Genode code,
	 * which then tries to perform IPC, the program fails because there exists
	 * no 'Thread_base' object. We recover from this unfortunate situation by
	 * creating a dummy 'Thread_base' object and associate it with the calling
	 * thread.
	 */

	/*
	 * Create dummy 'Thread_base' object but suppress the execution of its
	 * constructor. If we called the constructor, we would create a new Genode
	 * thread, which is not what we want. For the allocation, we use glibc
	 * malloc because 'Genode::env()->heap()->alloc()' uses IPC.
	 *
	 * XXX  Both the 'Thread_base' and 'Threadm_meta_data' objects are never
	 *      freed.
	 */
	Thread_base *thread = (Thread_base *)malloc(sizeof(Thread_base));
	memset(thread, 0, sizeof(*thread));
	Thread_meta_data *meta_data = new Thread_meta_data(thread);

	/*
	 * Initialize 'Thread_base::_tid' using the default constructor of
	 * 'Native_thread'. This marks the client and server sockets as
	 * uninitialized and prompts the IPC framework to create those as needed.
	 */
	meta_data->thread_base->tid() = Native_thread();
	adopt_thread(meta_data);

	return thread;
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
	_tid.meta_data = new (env()->heap()) Thread_meta_data(this);

	int const ret = pthread_create(&_tid.meta_data->pt, 0, thread_start,
	                              _tid.meta_data);
	if (ret) {
		PERR("pthread_create failed (returned %d, errno=%d)",
		     ret, errno);
		destroy(env()->heap(), _tid.meta_data);
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

	destroy(env()->heap(), _tid.meta_data);
	_tid.meta_data = 0;
}
