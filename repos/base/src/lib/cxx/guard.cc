/*
 * \brief  Support for guarded variables
 * \author Christian Helmuth
 * \date   2009-11-18
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/registry.h>
#include <base/semaphore.h>
#include <cpu/atomic.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/unmanaged_singleton.h>


typedef Genode::Registry<Genode::Registered_no_delete<Genode::Semaphore> > Blockers;


static Blockers *blockers_ptr;


void Genode::init_cxx_guard()
{
	blockers_ptr = unmanaged_singleton<Blockers>();
}


namespace __cxxabiv1
{
	enum State { INIT_NONE = 0, INIT_DONE = 1, IN_INIT = 0x100, WAITERS = 0x200 };

	/*
	 * A guarded variable can be in three states:
	 *
	 *   1) not initialized               - INIT_NONE
	 *   2) in initialization (transient) - IN_INIT and optionally WAITERS
	 *   3) initialized                   - INIT_DONE
	 *
	 * The generic ABI uses the first byte of a 64-bit guard variable for state
	 * 1), 2) and 3). ARM-EABI uses the first byte of a 32-bit guard variable.
	 * Therefore we define '__guard' as a 32-bit type and use the least
	 * significant byte for 1) and 3) and the following byte for 2) and let the
	 * other threads block until the guard is released by the thread in
	 * initialization. All waiting threads are stored in the 'blocked'
	 * registry and will be woken up by the thread releasing a guard.
	 */

	typedef int __guard;

	extern "C" int __cxa_guard_acquire(__guard *guard)
	{
		volatile char *initialized = (char *)guard;
		volatile int  *in_init     = (int *)guard;

		/* check for state 3) */
		if (*initialized == INIT_DONE) return 0;

		/* atomically check and set state 2) */
		if (!Genode::cmpxchg(in_init, INIT_NONE, IN_INIT)) {

			/* register current thread for blocking */
			Genode::Registered_no_delete<Genode::Semaphore> block(*blockers_ptr);

			/* tell guard thread that current thread needs a wakeup */
			while (!Genode::cmpxchg(in_init, *in_init, *in_init | WAITERS)) ;

			/* wait until state 3) is reached by guard thread */
			while (*initialized != INIT_DONE)
				block.down();

			/* guard not acquired */
			return 0;
		}

		/*
		 * The guard was acquired. The caller is allowed to initialize the
		 * guarded variable and required to call __cxa_guard_release() to flag
		 * initialization completion (and unblock other guard applicants).
		 */
		return 1;
	}


	extern "C" void __cxa_guard_release(__guard *guard)
	{
		volatile int  *in_init     = (int *)guard;

		/* set state 3) */
		while (!Genode::cmpxchg(in_init, *in_init, *in_init | INIT_DONE)) ;

		/* check whether somebody blocked on this guard */
		if (!(*in_init & WAITERS))
			return;

		/* we had contention - wake all up */
		blockers_ptr->for_each([](Genode::Registered_no_delete<Genode::Semaphore> &wake) {
			wake.up();
		});
	}


	extern "C" void __cxa_guard_abort(__guard *) {
		Genode::error(__func__, " called"); }
}
