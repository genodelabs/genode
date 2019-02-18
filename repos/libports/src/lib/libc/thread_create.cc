/*
 * \brief  pthread_create implementation
 * \author Christian Prochaska
 * \date   2012-03-12
 *
 * Purpose of a single file for pthread_create is that other application may
 * easily replace this implementation with another one.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "thread_create.h"
#include "thread.h"
#include <errno.h>


int Libc::pthread_create(pthread_t *thread,
                         void *(*start_routine) (void *), void *arg,
                         size_t stack_size, char const * name,
                         Genode::Cpu_session * cpu, Genode::Affinity::Location location)
{
		pthread_t thread_obj = new pthread(start_routine, arg,
		                                   stack_size, name, cpu, location);
		if (!thread_obj)
			return EAGAIN;

		*thread = thread_obj;

		thread_obj->start();

		return 0;
}


int Libc::pthread_create(pthread_t *thread, Genode::Thread &t)
{
		pthread_t thread_obj = new pthread(t);

		if (!thread_obj)
			return EAGAIN;

		*thread = thread_obj;
		return 0;
}


extern "C"
{
	int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	                   void *(*start_routine) (void *), void *arg)
	{
		size_t const stack_size = (attr && *attr && (*attr)->stack_size)
		                        ? (*attr)->stack_size
		                        : Libc::Component::stack_size();

		return Libc::pthread_create(thread, start_routine, arg, stack_size,
		                            "pthread", nullptr,
		                            Genode::Affinity::Location());
	}
}
