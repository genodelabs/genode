/*
 * \brief  Mutex primitive
 * \author Alexander Boettcher
 * \date   2020-01-24
 *
 * A 'Mutex' is a locking primitive designated for the mutual exclusion of
 * multiple threads executing a critical section, which is typically code that
 * mutates a shared variable.
 *
 * At initialization time, a mutex is in unlocked state. To enter and leave a
 * critical section, the methods 'acquire()' and 'release()' are provided.
 *
 * A mutex must not be used recursively. The subsequential attempt of aquiring
 * a mutex twice by the same thread ultimately results in a deadlock. This
 * misbehavior generates a warning message at runtime.
 *
 * Only the thread that aquired the mutex is permitted to release the mutex.
 * The violation of this invariant generates a warning message and leaves the
 * mutex unchanged.
 *
 * A 'Mutex::Guard' is provided, which acquires a mutex at construction time
 * and releases it automatically at the destruction time of the guard.
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__MUTEX_H_
#define _INCLUDE__BASE__MUTEX_H_

#include <base/lock.h>
#include <util/noncopyable.h>

namespace Genode { class Mutex; }


class Genode::Mutex : Noncopyable
{
	private:

		Lock _lock { Lock::UNLOCKED };

	public:

		Mutex() { }

		void acquire();
		void release();

		class Guard
		{
			private:

				Mutex &_mutex;

			public:

				explicit Guard(Mutex &mutex) : _mutex(mutex) { _mutex.acquire(); }

				~Guard() { _mutex.release(); }
		};
};

#endif /* _INCLUDE__BASE__MUTEX_H_ */
