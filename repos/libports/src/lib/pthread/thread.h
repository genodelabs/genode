/*
 * \brief  POSIX thread header
 * \author Christian Prochaska
 * \author Alexander Boettcher
 * \date   2012-03-12
 *
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SRC_LIB_PTHREAD_THREAD_H_
#define _INCLUDE__SRC_LIB_PTHREAD_THREAD_H_

#include <pthread.h>

extern "C" {

	struct pthread_attr
	{
		pthread_t pthread;

		pthread_attr() : pthread(0) { }
	};


	/*
	 * This class is named 'struct pthread' because the 'pthread_t' type is
	 * defined as 'struct pthread*' in '_pthreadtypes.h'
	 */
	struct pthread : Genode::Thread_base
	{
		pthread_attr_t _attr;
		void *(*_start_routine) (void *);
		void *_arg;

		enum { WEIGHT = Genode::Cpu_session::DEFAULT_WEIGHT };

		pthread(pthread_attr_t attr, void *(*start_routine) (void *),
		        void *arg, size_t stack_size, char const * name,
		        Genode::Cpu_session * cpu)
		: Thread_base(WEIGHT, name, stack_size, Type::NORMAL, cpu),
		  _attr(attr),
		  _start_routine(start_routine),
		  _arg(arg)
		{
			if (_attr)
				_attr->pthread = this;
		}

		/**
		 * Constructor to create pthread object out of existing thread,
		 * e.g. main Genode thread
		 */
		pthread(Thread_base &myself, pthread_attr_t attr)
		: Thread_base(myself),
		  _attr(attr), _start_routine(nullptr), _arg(nullptr)
		{
			if (_attr)
				_attr->pthread = this;
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
