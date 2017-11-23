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
	struct pthread : Genode::Thread
	{
		pthread_attr_t _attr;
		void *(*_start_routine) (void *);
		void *_arg;

		enum { WEIGHT = Genode::Cpu_session::Weight::DEFAULT_WEIGHT };

		pthread(pthread_attr_t attr, void *(*start_routine) (void *),
		        void *arg, size_t stack_size, char const * name,
		        Genode::Cpu_session * cpu, Genode::Affinity::Location location)
		: Thread(WEIGHT, name, stack_size, Type::NORMAL, cpu, location),
		  _attr(attr),
		  _start_routine(start_routine),
		  _arg(arg)
		{
			if (_attr)
				_attr->pthread = this;

			pthread_registry().insert(this);
		}

		/**
		 * Constructor to create pthread object out of existing thread,
		 * e.g. main Genode thread
		 */
		pthread(Thread &myself, pthread_attr_t attr)
		: Thread(myself),
		  _attr(attr), _start_routine(nullptr), _arg(nullptr)
		{
			if (_attr)
				_attr->pthread = this;

			pthread_registry().insert(this);
		}

		virtual ~pthread()
		{
			pthread_registry().remove(this);
		}

		void entry()
		{
			void *exit_status = _start_routine(_arg);
			pthread_exit(exit_status);
		}
	};

	void pthread_cleanup();
}

#endif /* _INCLUDE__SRC_LIB_PTHREAD_THREAD_H_ */
