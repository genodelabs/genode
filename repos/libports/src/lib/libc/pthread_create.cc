/*
 * \brief  pthread_create implementation
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2012-03-12
 *
 * Purpose of a single file for pthread_create is that other application may
 * easily replace this implementation with another one.
 */

/*
 * Copyright (C) 2012-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/lock.h>
#include <util/string.h>

/* libc includes */
#include <libc/allocator.h>
#include <errno.h>

/* libc-internal includes */
#include <internal/thread_create.h>
#include <internal/pthread.h>


static Genode::String<32> pthread_name()
{
	static Genode::Lock mutex;

	static unsigned id = 0;

	Genode::Lock::Guard guard(mutex);

	++id;

	return { "pthread.", id };
}


int Libc::pthread_create(pthread_t *thread,
                         void *(*start_routine) (void *), void *arg,
                         size_t stack_size, char const * name,
                         Cpu_session * cpu, Affinity::Location location)
{
	Libc::Allocator alloc { };
	pthread_t thread_obj = new (alloc)
	                       pthread(start_routine, arg,
	                               stack_size, name, cpu, location);
	if (!thread_obj)
		return EAGAIN;

	*thread = thread_obj;

	thread_obj->start();

	return 0;
}


int Libc::pthread_create(pthread_t *thread, Thread &t)
{
	Libc::Allocator alloc { };
	pthread_t thread_obj = new (alloc) pthread(t);

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
		                            pthread_name().string(), nullptr,
		                            Genode::Affinity::Location());
	}
}
