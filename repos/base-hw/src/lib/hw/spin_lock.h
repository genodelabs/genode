/*
 * \brief   Spin lock used to synchronize different CPU cores
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPIN_LOCK_H_
#define _SRC__LIB__HW__SPIN_LOCK_H_

#include <base/lock_guard.h>
#include <cpu/atomic.h>
#include <cpu/memory_barrier.h>

namespace Hw { class Spin_lock; }

class Hw::Spin_lock
{
	private:

		enum State { UNLOCKED, LOCKED };

		State volatile _locked = UNLOCKED;

	public:

		void lock()
		{
			while (!Genode::cmpxchg((volatile int*)&_locked, UNLOCKED, LOCKED))
				;
		}

		void unlock()
		{
			Genode::memory_barrier();
			_locked = UNLOCKED;
		}

		using Guard = Genode::Lock_guard<Spin_lock>;
};

#endif /* _SRC__LIB__HW__SPIN_LOCK_H_ */
