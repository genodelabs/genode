/*
 * \brief  POSIX readers/writer lock (rwlock) implementation
 * \author Alexander Senier
 * \author Christian Helmuth
 * \date   2018-01-25
 */

/*
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/mutex.h>
#include <base/thread.h>
#include <libc/allocator.h>

/* libc includes */
#include <errno.h>
#include <pthread.h>

/* libc-internal includes */
#include <internal/types.h>

using namespace Libc;


/*
 * A reader-preferring implementation of a readers-writer lock as described
 * in Michael Raynal, "Concurrent Programming: Algorithms, Principles, and
 * Foundations", ISBN 978-3-642-32026-2, page 75
 */

extern "C" {

	/*
	 * This class is named 'struct pthread_rwlock' because the 'pthread_rwlock_t'
	 * type is defined as 'struct rwlock*' in 'sys/_pthreadtypes.h'
	 */
	struct pthread_rwlock
	{
		private:

			Thread *_owner        { nullptr };
			Mutex   _nbr_mutex    { };
			Mutex   _global_mutex { };
			int _nbr = 0;

		public:

			void rdlock()
			{
				Mutex::Guard guard(_nbr_mutex);
				++_nbr;
				if (_nbr == 1) {
					_global_mutex.acquire();
					_owner = nullptr;
				}
			}

			void wrlock()
			{
				_global_mutex.acquire();
				_owner = Thread::myself();
			}

			int unlock()
			{
				/* Read lock */
				if (_owner == nullptr) {
					Mutex::Guard guard(_nbr_mutex);
					_nbr--;
					if (_nbr == 0) {
						_owner = nullptr;
						_global_mutex.release();
					}
					return 0;
				};

				if (_owner != Thread::myself()) {
					error("Unlocking writer lock owned by other thread");
					errno = EPERM;
					return -1;
				};

				/* Write lock owned by us */
				_owner = nullptr;
				_global_mutex.release();
				return 0;
			}
	};


	struct pthread_rwlockattr
	{
	};


	static int rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
	{
		static Mutex rwlock_init_mutex { };

		if (!rwlock)
			return EINVAL;

		try {
			Mutex::Guard g(rwlock_init_mutex);
			Libc::Allocator alloc { };
			*rwlock = new (alloc) struct pthread_rwlock();
			return 0;
		} catch (...) { return ENOMEM; }
	}


	int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
	{
		return rwlock_init(rwlock, attr);
	}

	typeof(pthread_rwlock_init) _pthread_rwlock_init
		__attribute__((alias("pthread_rwlock_init")));


	int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
	{
		Libc::Allocator alloc { };
		destroy(alloc, *rwlock);
		return 0;
	}

	typeof(pthread_rwlock_destroy) _pthread_rwlock_destroy
		__attribute__((alias("pthread_rwlock_destroy")));


	int pthread_rwlock_rdlock(pthread_rwlock_t * rwlock)
	{
		if (!rwlock)
			return EINVAL;

		if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
			if (rwlock_init(rwlock, NULL))
				return ENOMEM;

		(*rwlock)->rdlock();
		return 0;
	}

	typeof(pthread_rwlock_rdlock) _pthread_rwlock_rdlock
		__attribute__((alias("pthread_rwlock_rdlock")));


	int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
	{
		if (!rwlock)
			return EINVAL;

		if (*rwlock == PTHREAD_RWLOCK_INITIALIZER)
			if (rwlock_init(rwlock, NULL))
				return ENOMEM;

		(*rwlock)->wrlock();
		return 0;
	}

	typeof(pthread_rwlock_wrlock) _pthread_rwlock_wrlock
		__attribute__((alias("pthread_rwlock_wrlock")));


	int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
	{
		return (*rwlock)->unlock();
	}

	typeof(pthread_rwlock_unlock) _pthread_rwlock_unlock
		__attribute__((alias("pthread_rwlock_unlock")));


	int pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
	{
		Libc::Allocator alloc { };
		*attr = new (alloc) struct pthread_rwlockattr();
		return 0;
	}


	int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr, int *pshared)
	{
		*pshared = PTHREAD_PROCESS_PRIVATE;
		return 0;
	}


	int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared)
	{
		if (pshared != PTHREAD_PROCESS_PRIVATE) {
			errno = EINVAL;
			return -1;
		}
		return 0;
	}


	int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
	{
		Libc::Allocator alloc { };
		destroy(alloc, *attr);
		return 0;
	}


	/*
	 * Unimplemented functions:
	 *  int pthread_rwlock_timedrdlock(pthread_rwlock_t *, const struct timespec *);
	 *  int pthread_rwlock_timedwrlock(pthread_rwlock_t *, const struct timespec *);
	 *  int pthread_rwlock_tryrdlock(pthread_rwlock_t *);
	 *  int pthread_rwlock_trywrlock(pthread_rwlock_t *);
	 */
}
