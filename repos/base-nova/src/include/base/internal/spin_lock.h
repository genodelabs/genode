/*
 * \brief  Nova specific user land "Spin lock" implementation
 * \author Alexander Boettcher
 * \date   2014-02-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__SPIN_LOCK_H_
#define _INCLUDE__BASE__INTERNAL__SPIN_LOCK_H_

/* Genode includes */
#include <cpu/atomic.h>
#include <cpu/memory_barrier.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/lock_helper.h>


enum State {
	SPINLOCK_LOCKED = 0, SPINLOCK_UNLOCKED = 1, SPINLOCK_CONTENDED = 2,
};

enum { RESERVED_BITS = 12, COUNTER_MASK = 0xFFC };

template <typename T>
static inline void spinlock_lock(volatile T *lock_variable)
{
	using Genode::cmpxchg;

	Genode::Thread * myself = Genode::Thread::myself();
	T const tid = myself ? myself->native_thread().ec_sel : Nova::PT_SEL_MAIN_EC;

	unsigned help_counter = 0;

	/* sanity check that ec_sel fits into the lock_variable */
	if (tid >= (1 << (sizeof(*lock_variable) * 8 - RESERVED_BITS)))
		nova_die();

	if (myself) {
		Nova::Utcb * utcb  = reinterpret_cast<Nova::Utcb *>(myself->utcb());
		help_counter       = utcb->tls & COUNTER_MASK;
	}

	/* try to get lock */
	do {
		T raw = *lock_variable;

		if (raw != SPINLOCK_UNLOCKED) {
			if (!(raw & SPINLOCK_CONTENDED))
				/* if it fails - just re-read and retry */
				if (!Genode::cmpxchg(lock_variable, raw, raw | SPINLOCK_CONTENDED))
					continue;

			/*
			 * Donate remaining time slice to help the spinlock holder to
			 * pass the critical section.
			 */
			unsigned long const ec  = raw >> RESERVED_BITS;
			unsigned long const tls = raw & COUNTER_MASK;
			Nova::ec_ctrl(Nova::EC_DONATE_SC, ec, tls);
			continue;
		}
	} while (!cmpxchg(lock_variable, (T)SPINLOCK_UNLOCKED,
	                  (tid << RESERVED_BITS) | help_counter | SPINLOCK_LOCKED));
}


template <typename T>
static inline void spinlock_unlock(volatile T *lock_variable)
{
	using Nova::Utcb;

	Genode::Thread * myself = Genode::Thread::myself();
	Utcb * utcb = myself ? reinterpret_cast<Utcb *>(myself->utcb()) : 0;

	/* unlock */
	T old;
	do {
		old = *lock_variable;
	} while (!Genode::cmpxchg(lock_variable, old, (T)SPINLOCK_UNLOCKED));

	/* de-flag time donation help request and set new counter */
	if (utcb) {
		utcb->tls = (((utcb->tls & COUNTER_MASK) + 4) % 4096) & COUNTER_MASK;
		/* take care that compiler generates code that writes tls to memory */
		Genode::memory_barrier();
	}

	/*
	 * If anybody donated time, request kernel for a re-schedule in order that
	 * the helper can get its time donation (SC) back.
	 */
	if (old & SPINLOCK_CONTENDED)
		Nova::ec_ctrl(Nova::EC_RESCHEDULE);
}

#endif /* _INCLUDE__BASE__INTERNAL__SPIN_LOCK_H_ */
