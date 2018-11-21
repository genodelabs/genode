/*
 * \brief  POSIX readers/writer lock (rwlock) implementation
 * \author Alexander Senier
 * \date   2018-01-25
 *
 */

/*
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/lock.h>
#include <base/lock_guard.h>
#include <base/thread.h>

/* Libc includes */
#include <errno.h>
#include <pthread.h>

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

			Genode::Thread *_owner = nullptr;
			Genode::Lock _nbr_mutex {};
			Genode::Lock _global_mutex {};
			int _nbr = 0;

		public:

			void rdlock()
			{
				Genode::Lock_guard<Genode::Lock> guard(_nbr_mutex);
				++_nbr;
				if (_nbr == 1) {
					_global_mutex.lock();
					_owner = nullptr;
				}
			}

			void wrlock()
			{
				_global_mutex.lock();
				_owner = Genode::Thread::myself();
			}

			int unlock()
			{
				/* Read lock */
				if (_owner == nullptr) {
					Genode::Lock_guard<Genode::Lock> guard(_nbr_mutex);
					_nbr--;
					if (_nbr == 0) {
						_owner = nullptr;
						_global_mutex.unlock();
					}
					return 0;
				};

				if (_owner != Genode::Thread::myself()) {
					Genode::error("Unlocking writer lock owned by other thread");
					errno = EPERM;
					return -1;
				};

				/* Write lock owned by us */
				_owner = nullptr;
				_global_mutex.unlock();
				return 0;
			}
	};

	struct pthread_rwlockattr
	{
	};

	int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
	{
		*rwlock = new struct pthread_rwlock();
		return 0;
	}

	int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
	{
		delete *rwlock;
		return 0;
	}

	int pthread_rwlock_rdlock(pthread_rwlock_t * rwlock)
	{
		(*rwlock)->rdlock();
		return 0;
	}

	int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
	{
		(*rwlock)->wrlock();
		return 0;
	}

	int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
	{
		return (*rwlock)->unlock();
	}

	int pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
	{
		*attr = new struct pthread_rwlockattr();
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
		delete *attr;
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
