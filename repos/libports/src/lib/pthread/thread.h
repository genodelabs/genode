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

#ifndef _INCLUDE__SRC_LIB_PTHREAD_THREAD_H_
#define _INCLUDE__SRC_LIB_PTHREAD_THREAD_H_

/* Genode includes */
#include <util/reconstructible.h>

#include <pthread.h>

/*
 * Used by 'pthread_self()' to find out if the current thread is an alien
 * thread.
 */
class Pthread_registry
{
	private:

		enum { MAX_NUM_PTHREADS = 128 };

		pthread_t _array[MAX_NUM_PTHREADS] = { 0 };

	public:

		void insert(pthread_t thread);

		void remove(pthread_t thread);

		bool contains(pthread_t thread);
};


Pthread_registry &pthread_registry();


extern "C" {

	struct pthread_attr
	{
		pthread_t pthread;
		size_t stack_size;

		pthread_attr() : pthread(0), stack_size(0) { }
	};

	/*
	 * This class is named 'struct pthread' because the 'pthread_t' type is
	 * defined as 'struct pthread*' in '_pthreadtypes.h'
	 */
	struct pthread;

	void pthread_cleanup();
}


struct Genode::Thread::Tls::Base
{
	/**
	 * Register thread-local-storage object at Genode thread
	 */
	static void tls(Genode::Thread &thread, Tls::Base &tls)
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


struct pthread : Genode::Noncopyable, Genode::Thread::Tls::Base
{
	typedef void *(*start_routine_t) (void *);

	private:

		struct Thread_object : Genode::Thread
		{
			start_routine_t _start_routine;
			void           *_arg;
			bool            _exiting = false;

			enum { WEIGHT = Genode::Cpu_session::Weight::DEFAULT_WEIGHT };

			Thread_object(char const *name, size_t stack_size,
			              Genode::Cpu_session *cpu,
			              Genode::Affinity::Location location,
			              start_routine_t start_routine, void *arg)
			:
				Genode::Thread(WEIGHT, name, stack_size, Type::NORMAL, cpu, location),
				_start_routine(start_routine), _arg(arg)
			{ }

			void entry() override;
		};

		Genode::Constructible<Thread_object> _thread_object;

		/*
		 * Helper to construct '_thread_object' in class initializer
		 */
		template <typename... ARGS>
		Genode::Thread &_construct_thread_object(ARGS &&... args)
		{
			_thread_object.construct(args...);
			return *_thread_object;
		}

		/*
		 * Refers to '_thread_object' or an external 'Genode::Thread' object
		 */
		Genode::Thread &_thread;

		void _associate_thread_with_pthread()
		{
			Genode::Thread::Tls::Base::tls(_thread, *this);
			pthread_registry().insert(this);
		}

	public:

		/**
		 * Constructor for threads created via 'pthread_create'
		 */
		pthread(start_routine_t start_routine,
		        void *arg, size_t stack_size, char const * name,
		        Genode::Cpu_session * cpu, Genode::Affinity::Location location)
		:
			_thread(_construct_thread_object(name, stack_size, cpu, location,
			                                 start_routine, arg))
		{
			_associate_thread_with_pthread();
		}

		/**
		 * Constructor to create pthread object out of existing thread,
		 * i.e., the main thread
		 */
		pthread(Genode::Thread &existing_thread)
		:
			_thread(existing_thread)
		{
			_associate_thread_with_pthread();
		}

		~pthread()
		{
			pthread_registry().remove(this);
		}

		void start() { _thread.start(); }
		bool exiting() const
		{
			return _thread_object->_exiting;
		}

		void join() { _thread.join(); }

		void *stack_top()  const { return _thread.stack_top();  }
		void *stack_base() const { return _thread.stack_base(); }
};

#endif /* _INCLUDE__SRC_LIB_PTHREAD_THREAD_H_ */
