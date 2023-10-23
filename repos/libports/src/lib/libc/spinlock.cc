/*
 * \brief  libc-internal spinlock implementation
 * \author Christian Prochaska
 * \date   2023-10-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <pthread.h>
#include <spinlock.h>

extern "C" {

	/*
	 * Only one spinlock ('__stdio_thread_lock') is
	 * used in the FreeBSD libc code.
	 */

	static pthread_mutex_t *stdio_thread_lock_mutex()
	{
		static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
		return &mutex;
	}

	void _spinlock(spinlock_t *)
	{
		pthread_mutex_lock(stdio_thread_lock_mutex());
	}

	typeof(_spinlock) __sys_spinlock
		__attribute__((alias("_spinlock")));

	void _spinunlock(spinlock_t *)
	{
		pthread_mutex_unlock(stdio_thread_lock_mutex());
	}

	typeof(_spinunlock) __sys_spinunlock
		__attribute__((alias("_spinunlock")));
}
