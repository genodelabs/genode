/*
 * \brief  Supplemental code for hybrid Genode/Linux components
 * \author Norman Feske
 * \date   2011-09-02
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <linux_syscalls.h>
#include <linux_native_cpu/client.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/globals.h>
#include <base/internal/platform.h>


/**
 * Define stack area
 */
Genode::addr_t _stack_area_start;


enum { verbose_atexit = false };


/**
 * Dummy for symbol that is normally provided by '_main.cc'
 */
int genode___cxa_atexit(void (*)(void*), void *, void *)
{
	if (verbose_atexit)
		Genode::raw("genode___cxa_atexit called, not implemented\n");
	return 0;
}


/*
 * Manually initialize the 'lx_environ' pointer. For non-hybrid programs, this
 * pointer is initialized by the startup code.
 */
extern char **environ;
extern char **lx_environ;

static char signal_stack[0x2000] __attribute__((aligned(0x1000)));

static void empty_signal_handler(int) { }

extern void lx_exception_signal_handlers();

/*
 * This function must be called before any other static constructor in the Genode
 * application, so it gets the highest priority (lowest priority number >100)
 */
__attribute__((constructor(101))) void lx_hybrid_init()
{
	lx_environ = environ;

	lx_sigaltstack(signal_stack, sizeof(signal_stack));
	lx_exception_signal_handlers();

	/*
	 * Set signal handler such that canceled system calls get not
	 * transparently retried after a signal gets received.
	 */
	lx_sigaction(LX_SIGUSR1, empty_signal_handler, false);
}


static Genode::Env *_env_ptr;


namespace Genode {

	extern void bootstrap_component();
	extern void call_global_static_constructors();

	struct Thread_meta_data_created;
	struct Thread_meta_data_adopted;
}


/*
 * This function is normally provided by the cxx library, which is not
 * used for lx_hybrid programs. For lx_hybrid programs, the exception
 * handling is initialized by the host system's regular startup code.
 *
 * However, we conveniently use this function to get hold of the
 * component's environment and initialize the default log output.
 */
void Genode::init_exception_handling(Env &env)
{
	_env_ptr = &env;

	init_log(env.parent());
}


/*
 * Static constructors are handled by the Linux startup code - so implement
 * this as empty function.
 */
void Genode::call_global_static_constructors() { }


Genode::size_t Component::stack_size() __attribute__((weak));
Genode::size_t Component::stack_size()
{
	return 16UL * 1024 * sizeof(Genode::addr_t);
}


/*
 * Hybrid components are not allowed to implement legacy main(). This enables
 * us to hook in and bootstrap components as usual.
 */

int main()
{
	Genode::init_platform();
	Genode::bootstrap_component();

	/* never reached */
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
#include <base/component.h>

/* host libc includes */
#define size_t __SIZE_TYPE__ /* see comment in 'linux_syscalls.h' */
#pragma GCC diagnostic ignored "-Weffc++"
#include <pthread.h>
#pragma GCC diagnostic pop
#include <stdio.h>
#include <errno.h>
#undef size_t

using namespace Genode;


/**
 * Return TLS key used to storing the thread meta data
 */
static pthread_key_t tls_key()
{
	struct Tls_key
	{
		pthread_key_t key { };

		Tls_key()
		{
			pthread_key_create(&key, 0);
		}
	};

	static Tls_key inst;
	return inst.key;
}


/**
 * Meta data tied to the thread via the pthread TLS mechanism
 */
struct Genode::Native_thread::Meta_data
{
	/**
	 * Linux-specific thread meta data
	 *
	 * For non-hybrid programs, this information is located at the
	 * 'Stack'. But the POSIX threads of hybrid programs have no 'Stack'
	 * object. So we have to keep the meta data here.
	 */
	Native_thread native_thread { };

	/**
	 * Filled out by 'thread_start' function in the stack of the new
	 * thread
	 */
	Thread &thread_base;

	/**
	 * POSIX thread handle
	 */
	pthread_t pt { };

	/**
	 * Constructor
	 *
	 * \param thread  associated 'Thread' object
	 */
	Meta_data(Thread *thread) : thread_base(*thread)
	{
		native_thread.meta_data = this;
	}

	virtual ~Meta_data() { }

	/**
	 * Used to block the constructor until the new thread has initialized
	 * 'id'
	 */
	virtual void wait_for_construction() = 0;
	virtual void constructed() = 0;

	/**
	 * Used to block the new thread until 'start' is called
	 */
	virtual void wait_for_start() = 0;
	virtual void started() = 0;

	/**
	 * Used to block the 'join()' function until the 'entry()' is done
	 */
	virtual void wait_for_join() = 0;
	virtual void joined() = 0;
};


/**
 * Thread meta data for a thread created by Genode
 */
class Genode::Thread_meta_data_created : public Native_thread::Meta_data
{
	private:

		/**
		 * Used to block the constructor until the new thread has initialized
		 * 'id'
		 */
		Blockade _construct_lock { };

		/**
		 * Used to block the new thread until 'start' is called
		 */
		Blockade _start_lock { };

		/**
		 * Used to block the 'join()' function until the 'entry()' is done
		 */
		Blockade _join_lock { };

	public:

		Thread_meta_data_created(Thread *thread)
		: Native_thread::Meta_data(thread) { }

		void wait_for_construction() override
		{
			_construct_lock.block();
		}

		void constructed() override
		{
			_construct_lock.wakeup();
		}

		void wait_for_start() override
		{
			_start_lock.block();
		}

		void started() override
		{
			_start_lock.wakeup();
		}

		void wait_for_join() override
		{
			_join_lock.block();
		}

		void joined() override
		{
			_join_lock.wakeup();
		}
};


/**
 * Thread meta data for an adopted thread
 */
class Genode::Thread_meta_data_adopted : public Native_thread::Meta_data
{
	public:

		Thread_meta_data_adopted(Thread *thread)
		: Native_thread::Meta_data(thread) { }

		void wait_for_construction() override
		{
			error("wait_for_construction() called for an adopted thread");
		}

		void constructed() override
		{
			error("constructed() called for an adopted thread");
		}

		void wait_for_start() override
		{
			error("wait_for_start() called for an adopted thread");
		}

		void started() override
		{
			error("started() called for an adopted thread");
		}

		void wait_for_join() override
		{
			error("wait_for_join() called for an adopted thread");
		}

		void joined() override
		{
			error("joined() called for an adopted thread");
		}
};


static void adopt_thread(Native_thread::Meta_data *meta_data)
{
	lx_sigaltstack(signal_stack, sizeof(signal_stack));

	/*
	 * Set signal handler such that canceled system calls get not
	 * transparently retried after a signal gets received.
	 */
	lx_sigaction(LX_SIGUSR1, empty_signal_handler, false);

	/*
	 * Prevent children from becoming zombies. (SIG_IGN = 1)
	 */
	lx_sigaction(LX_SIGCHLD, (void (*)(int))1, false);

	/* assign 'Native_thread::Meta_data' pointer to TLS entry */
	pthread_setspecific(tls_key(), meta_data);

	/* enable immediate cancellation when calling 'pthread_cancel' */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	/*
	 * Initialize thread meta data
	 */
	Native_thread &native_thread = meta_data->thread_base.native_thread();
	native_thread.tid = lx_gettid();
	native_thread.pid = lx_getpid();
}


static void *thread_start(void *arg)
{
	Native_thread::Meta_data *meta_data = (Native_thread::Meta_data *)arg;

	adopt_thread(meta_data);

	/* unblock 'Thread' constructor */
	meta_data->constructed();

	/* block until the 'Thread::start' gets called */
	meta_data->wait_for_start();

	Thread::myself()->entry();

	meta_data->joined();
	return 0;
}


void Genode::init_thread(Cpu_session &, Region_map &)  { }
void Genode::init_thread_start(Capability<Pd_session>) { }
void Genode::init_thread_bootstrap(Thread_capability)  { }


extern "C" void *malloc(::size_t size);
extern "C" void free(void *);

namespace {

	struct Global_allocator : Allocator
	{
		typedef Genode::size_t size_t;

		Alloc_result try_alloc(size_t size) override { return malloc(size); }

		void free(void *addr, size_t) override { ::free(addr); }

		bool need_size_for_free() const override { return false; }

		size_t overhead(size_t) const override { return 0; }
	};
}


static Allocator &global_alloc()
{
	static Global_allocator inst;
	return inst;
}


Thread *Thread::myself()
{
	void * const tls = pthread_getspecific(tls_key());

	if (tls != 0)
		return &((Native_thread::Meta_data *)tls)->thread_base;

	bool const called_by_main_thread = (lx_getpid() == lx_gettid());
	if (called_by_main_thread)
		return 0;

	/*
	 * The function was called from a thread created by other means than
	 * Genode's thread API. This may happen if a native Linux library creates
	 * threads via the pthread library. If such a thread calls Genode code,
	 * which then tries to perform IPC, the program fails because there exists
	 * no 'Thread' object. We recover from this unfortunate situation by
	 * creating a dummy 'Thread' object and associate it with the calling
	 * thread.
	 */

	/*
	 * Create dummy 'Thread' object but suppress the execution of its
	 * constructor. If we called the constructor, we would create a new Genode
	 * thread, which is not what we want. For the allocation, we use glibc
	 * malloc because 'Genode::env()->heap()->alloc()' uses IPC.
	 *
	 * XXX  Both the 'Thread' and 'Native_thread::Meta_data' objects are
	 *      never freed.
	 */
	Thread *thread = (Thread *)malloc(sizeof(Thread));
	memset(thread, 0, sizeof(*thread));
	Native_thread::Meta_data *meta_data =
		new (global_alloc()) Thread_meta_data_adopted(thread);

	/*
	 * Initialize 'Thread::_native_thread' to point to the default-
	 * constructed 'Native_thread' (part of 'Meta_data').
	 */
	meta_data->thread_base._native_thread = &meta_data->native_thread;
	adopt_thread(meta_data);

	return thread;
}


void Thread::start()
{
	/*
	 * Unblock thread that is supposed to slumber in 'thread_start'.
	 */
	native_thread().meta_data->started();
}


void Thread::join()
{
	native_thread().meta_data->wait_for_join();
}


Native_thread &Thread::native_thread() { return *_native_thread; }


Thread::Thread(size_t weight, const char *name, size_t /* stack size */,
               Type, Cpu_session * cpu_sess, Affinity::Location)
: _cpu_session(cpu_sess), _affinity()
{
	Native_thread::Meta_data *meta_data =
		new (global_alloc()) Thread_meta_data_created(this);

	_native_thread = &meta_data->native_thread;

	int const ret = pthread_create(&meta_data->pt, 0, thread_start, meta_data);
	if (ret) {
		error("pthread_create failed (returned ", ret, ", errno=", errno, ")");
		destroy(global_alloc(), meta_data);
		throw Out_of_stack_space();
	}

	native_thread().meta_data->wait_for_construction();

	_thread_cap = _cpu_session->create_thread(_env_ptr->pd_session_cap(), name,
	                                          Location(), Weight(weight));

	Linux_native_cpu_client native_cpu(_cpu_session->native_cpu());
	native_cpu.thread_id(_thread_cap, native_thread().pid, native_thread().tid);
}


Thread::Thread(size_t weight, const char *name, size_t stack_size,
               Type type, Affinity::Location)
: Thread(weight, name, stack_size, type, &_env_ptr->cpu()) { }


Thread::Thread(Env &, Name const &name, size_t stack_size, Location location,
               Weight weight, Cpu_session &cpu)
: Thread(weight.value, name.string(), stack_size, NORMAL,
         &cpu, location)
{ }


Thread::Thread(Env &env, Name const &name, size_t stack_size)
: Thread(env, name, stack_size, Location(), Weight(), env.cpu()) { }


Thread::~Thread()
{
	bool const needs_join = (pthread_cancel(native_thread().meta_data->pt) == 0);

	if (needs_join) {
		int const ret = pthread_join(native_thread().meta_data->pt, 0);
		if (ret)
			warning("pthread_join unexpectedly returned "
			        "with ", ret, " (errno=", errno, ")");
	}

	Thread_meta_data_created *meta_data =
		dynamic_cast<Thread_meta_data_created *>(native_thread().meta_data);

	if (meta_data)
		destroy(global_alloc(), meta_data);

	_native_thread = nullptr;

	/* inform core about the killed thread */
	_cpu_session->kill_thread(_thread_cap);
}


/**************
 ** Platform **
 **************/

void Platform::_attach_stack_area()
{
	/*
	 * Omit attaching the stack area to the local address space for hybrid
	 * components. Otherwise, it may collide with the (randomized) loading
	 * locations of shared objects or the binary.
	 */
}
