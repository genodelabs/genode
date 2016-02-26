/*
 * \brief   Kernel lock
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__LOCK_H_
#define _KERNEL__LOCK_H_

/* Genode includes */
#include <base/lock_guard.h>
#include <cpu/atomic.h>
#include <cpu/memory_barrier.h>

namespace Kernel
{
	/**
	 * Lock that enables synchronization inside the kernel
	 */
	class Lock;

	Lock & data_lock();
}

class Kernel::Lock
{
	private:

		int volatile _locked;

	public:

		Lock() : _locked(0) { }

		/**
		 * Request the lock
		 */
		void lock() { while (!Genode::cmpxchg(&_locked, 0, 1)); }

		/**
		 * Free the lock
		 */
		void unlock()
		{
			Genode::memory_barrier();
			_locked = 0;
		}

		/**
		 * Provide guard semantic for this type of lock
		 */
		typedef Genode::Lock_guard<Kernel::Lock> Guard;
};

#endif /* _KERNEL__LOCK_H_ */
