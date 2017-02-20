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

#include <cpu/atomic.h>

namespace __cxxabiv1 
{

	/*
	 * A guarded variable can be in three states:
	 *
	 *   1) not initialized
	 *   2) in initialization (transient)
	 *   3) initialized
	 *
	 * The generic ABI uses the first byte of a 64-bit guard variable for state
	 * 1), 2) and 3). ARM-EABI uses the first byte of a 32-bit guard variable.
	 * Therefore we define '__guard' as a 32-bit type and use the least
	 * significant byte for 1) and 3) and the following byte for 2) and let the
	 * other threads spin until the guard is released by the thread in
	 * initialization.
	 */

	typedef int __guard;

	extern "C" int __cxa_guard_acquire(__guard *guard)
	{
		volatile char *initialized = (char *)guard;
		volatile int  *in_init     = (int *)guard;

		/* check for state 3) */
		if (*initialized) return 0;

		/* atomically check and set state 2) */
		if (!Genode::cmpxchg(in_init, 0, 0x100)) {
			/* spin until state 3) is reached if other thread is in init */
			while (!*initialized) ;

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
		volatile char *initialized = (char *)guard;

		/* set state 3) */
		*initialized = 1;
	}


	extern "C" void __cxa_guard_abort(__guard *) { }
}
