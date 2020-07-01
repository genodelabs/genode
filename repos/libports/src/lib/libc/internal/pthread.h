/*
 * \brief  POSIX thread header
 * \author Christian Prochaska
 * \author Alexander Boettcher
 * \date   2012-03-12
 *
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__PTHREAD_H_
#define _LIBC__INTERNAL__PTHREAD_H_

/* Genode includes */
#include <base/blockade.h>
#include <libc/allocator.h>
#include <libc/component.h>
#include <util/reconstructible.h>

/* libc includes */
#include <pthread.h>

/* libc-internal includes */
#include <internal/types.h>
#include <internal/monitor.h>
#include <internal/timer.h>

namespace Libc {

	struct Pthread;
	struct Pthread_registry;
	struct Pthread_blockade;
	struct Pthread_job;
}


/*
 * Used by 'pthread_self()' to find out if the current thread is an alien
 * thread.
 */
class Libc::Pthread_registry
{
	private:

		enum { MAX_NUM_PTHREADS = 128 };

		Pthread *_array[MAX_NUM_PTHREADS] = { 0 };

	public:

		void insert(Pthread &thread);

		void remove(Pthread &thread);

		bool contains(Pthread &thread);
};


Libc::Pthread_registry &pthread_registry();


extern "C" {

	struct pthread_attr
	{
		void   *stack_addr { nullptr };
		size_t  stack_size { Libc::Component::stack_size() };
	};

	/*
	 * This class is named 'struct pthread' because the 'pthread_t' type is
	 * defined as 'struct pthread*' in '_pthreadtypes.h'
	 */
	struct pthread;
}


struct Genode::Thread::Tls::Base
{
	/**
	 * Register thread-local-storage object at Genode thread
	 */
	static void tls(Thread &thread, Tls::Base &tls)
	{
		thread._tls = Tls { &tls };
	}

	struct Undefined : Exception { };

	/**
	 * Obtain thread-local-storage object for the calling thread
	 *
	 * \throw Undefined
	 */
	static Tls::Base &tls()
	{
		Thread &myself = *Thread::myself();

		if (!myself._tls.ptr)
			throw Undefined();

		return *myself._tls.ptr;
	}
};


struct Libc::Pthread : Noncopyable, Thread::Tls::Base
{
	typedef void *(*start_routine_t) (void *);

	private:

		struct Thread_object : Thread
		{
			start_routine_t _start_routine;
			void           *_arg;

			void          *&_stack_addr;
			size_t         &_stack_size;

			enum { WEIGHT = Cpu_session::Weight::DEFAULT_WEIGHT };

			/* 'stack_addr_out' and 'stack_size_out' are written when the thread starts */
			Thread_object(char const *name, size_t stack_size,
			              Cpu_session *cpu,
			              Affinity::Location location,
			              start_routine_t start_routine, void *arg,
			              void *&stack_addr_out, size_t &stack_size_out)
			:
				Thread(WEIGHT, name, stack_size, Type::NORMAL, cpu, location),
				_start_routine(start_routine), _arg(arg),
				_stack_addr(stack_addr_out), _stack_size(stack_size_out)
			{ }

			void entry() override;
		};

		Constructible<Thread_object> _thread_object;

		/*
		 * Helper to construct '_thread_object' in class initializer
		 */
		template <typename... ARGS>
		Thread &_construct_thread_object(ARGS &&... args)
		{
			_thread_object.construct(args...);
			return *_thread_object;
		}

		/*
		 * Refers to '_thread_object' or an external 'Thread' object
		 */
		Thread &_thread;

		void _associate_thread_with_pthread()
		{
			Thread::Tls::Base::tls(_thread, *this);
			pthread_registry().insert(*this);
		}

		bool _exiting = false;

		/*
		 * The join blockade is needed because 'Libc::resume_all()' uses a
		 * 'Signal_transmitter' which holds a reference to a signal context
		 * capability, which needs to be released before the thread can be
		 * destroyed.
		 *
		 * Also, we cannot use 'Thread::join()', because it only
		 * returns when the thread entry function returns, which does not
		 * happen with 'pthread_cancel()'.
		 */
		Genode::Blockade _join_blockade;

		/* return value for 'pthread_join()' */
		void *_retval = PTHREAD_CANCELED;

		/* attributes for 'pthread_attr_get_np()' */
		void   *_stack_addr = nullptr;
		size_t  _stack_size = 0;

		/* cleanup handlers */

		class Cleanup_handler : public List<Cleanup_handler>::Element
		{
			private:
				void (*_routine)(void*);
				void *_arg;

			public:
				Cleanup_handler(void (*routine)(void*), void *arg)
				: _routine(routine), _arg(arg) { }

				void execute() { _routine(_arg); }
		};

		List<Cleanup_handler> _cleanup_handlers;

	public:

		int thread_local_errno = 0;

		/**
		 * Constructor for threads created via 'pthread_create'
		 */
		Pthread(start_routine_t start_routine,
		        void *arg, size_t stack_size, char const * name,
		        Cpu_session * cpu, Affinity::Location location)
		:
			_thread(_construct_thread_object(name, stack_size, cpu, location,
			                                 start_routine, arg,
			                                 _stack_addr, _stack_size))
		{
			_associate_thread_with_pthread();
		}

		/**
		 * Constructor to create pthread object out of existing thread,
		 * i.e., the main thread
		 */
		Pthread(Thread &existing_thread)
		:
			_thread(existing_thread)
		{
			/* obtain stack attributes of main thread */
			Thread::Stack_info info = Thread::mystack();
			_stack_addr = (void *)info.base;
			_stack_size = info.top - info.base;

			_associate_thread_with_pthread();
		}

		~Pthread()
		{
			pthread_registry().remove(*this);
		}

		void start() { _thread.start(); }

		void join(void **retval);

		/*
		 * Inform the thread calling 'pthread_join()' that this thread can be
		 * destroyed.
		 */
		void cancel();

		void exit(void *retval)
		{
			while (cleanup_pop(1)) { }
			_retval = retval;
			cancel();
		}

		void   *stack_addr() const { return _stack_addr; }
		size_t  stack_size() const { return _stack_size; }

		/*
		 * Push a cleanup handler to the cancellation cleanup stack.
		 */
		void cleanup_push(void (*routine)(void*), void *arg)
		{
			Libc::Allocator alloc { };
			_cleanup_handlers.insert(new (alloc) Cleanup_handler(routine, arg));
		}

		/*
		 * Pop and optionally execute the top-most cleanup handler.
		 * Returns true if a handler was found.
		 */
		bool cleanup_pop(int execute)
		{
			Cleanup_handler *cleanup_handler = _cleanup_handlers.first();

			if (!cleanup_handler) return false;

			_cleanup_handlers.remove(cleanup_handler);

			if (execute)
				cleanup_handler->execute();

			Libc::Allocator alloc { };
			destroy(alloc, cleanup_handler);

			return true;
		}
};


struct pthread : Libc::Pthread
{
	using Libc::Pthread::Pthread;
};


class Libc::Pthread_blockade : public Blockade, public Timeout_handler
{
	private:

		Genode::Blockade       _blockade;
		Constructible<Timeout> _timeout;

	public:

		Pthread_blockade(Timer_accessor &timer_accessor, uint64_t timeout_ms)
		{
			if (timeout_ms) {
				_timeout.construct(timer_accessor, *this);
				_timeout->start(timeout_ms);
			}
		}

		void block() override { _blockade.block(); }

		void wakeup() override
		{
			_woken_up = true;
			_blockade.wakeup();
		}

		/**
		 * Timeout_handler interface
		 */
		void handle_timeout() override
		{
			_expired = true;
			_blockade.wakeup();
		}
};


struct Libc::Pthread_job : Monitor::Job
{
	private:

		Pthread_blockade _blockade;

	public:

		Pthread_job(Monitor::Function &fn,
		            Timer_accessor &timer_accessor, uint64_t timeout_ms)
		:
			Job(fn, _blockade),
			_blockade(timer_accessor, timeout_ms)
		{ }
};


#endif /* _LIBC__INTERNAL__PTHREAD_H_ */
