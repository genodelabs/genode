/*
 * \brief  Spin lock implementation
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2009-03-25
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__SPIN_LOCK_H_
#define _INCLUDE__BASE__INTERNAL__SPIN_LOCK_H_

/* Genode includes */
#include <cpu/atomic.h>
#include <cpu/memory_barrier.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/lock_helper.h>

/*
 * Spinlock functions used for protecting the critical sections within the
 * 'lock' and 'unlock' functions. Contention in these short-running code
 * portions is rare but is must be considered.
 */

enum State { SPINLOCK_LOCKED, SPINLOCK_UNLOCKED };


static inline void spinlock_lock(volatile int *lock_variable)
{
	while (!Genode::cmpxchg(lock_variable, SPINLOCK_UNLOCKED, SPINLOCK_LOCKED)) {
		/*
		 * Yield our remaining time slice to help the spinlock holder to pass
		 * the critical section.
		 */
		thread_yield();
	}
}


static inline void spinlock_unlock(volatile int *lock_variable)
{
	/* make sure all got written by compiler before releasing lock */
	Genode::memory_barrier();
	*lock_variable = SPINLOCK_UNLOCKED;
}

#endif /* _INCLUDE__BASE__INTERNAL__SPIN_LOCK_H_ */
