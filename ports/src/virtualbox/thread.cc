/*
 * \brief  Virtualbox adjusted pthread_create implementation
 * \author Alexander Boettcher
 * \date   2014-04-09
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode */
#include <base/printf.h>
#include <base/thread.h>
#include <base/env.h>

/* Genode libc pthread binding */
#include "thread.h"

/* libc */
#include <pthread.h>
#include <errno.h>

/* vbox */
#include <internal/thread.h>

extern "C" {

	int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	                   void *(*start_routine) (void *), void *arg)
	{
		PRTTHREADINT rtthread = reinterpret_cast<PRTTHREADINT>(arg);

		Assert(rtthread);

		size_t stack_size = Genode::Native_config::context_virtual_size() -
		                    sizeof(Genode::Native_utcb) - 2 * (1UL << 12);

		if (rtthread->cbStack < stack_size)
			stack_size = rtthread->cbStack;
		else
			PWRN("requested stack for thread '%s' of %zu Bytes is too large, "
			     "limit to %zu Bytes", rtthread->szName, rtthread->cbStack,
			     stack_size);
		
		pthread_t thread_obj = new (Genode::env()->heap())
		                           pthread(attr ? *attr : 0, start_routine,
		                           arg, stack_size, rtthread->szName, nullptr);

		if (!thread_obj)
			return EAGAIN;

		*thread = thread_obj;

		thread_obj->start();

		return 0;
	}
}
