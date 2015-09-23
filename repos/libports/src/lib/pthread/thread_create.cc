/*
 * \brief  pthread_create implementation
 * \author Christian Prochaska
 * \date   2012-03-12
 *
 * Purpose of a single file for pthread_create is that other application may
 * easily replace this implementation with another one.
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>

#include <errno.h>
#include <pthread.h>
#include "thread.h"

extern "C"
{
	enum { STACK_SIZE=64*1024 };

	int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	                   void *(*start_routine) (void *), void *arg)
	{
		/* cleanup threads which tried to self-destruct */
		pthread_cleanup();

		pthread_t thread_obj = new (Genode::env()->heap())
		                           pthread(attr ? *attr : 0, start_routine,
		                           arg, STACK_SIZE, "pthread", nullptr);

		if (!thread_obj)
			return EAGAIN;

		*thread = thread_obj;

		thread_obj->start();

		return 0;
	}
}
