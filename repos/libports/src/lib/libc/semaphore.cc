/*
 * \brief  POSIX semaphore implementation
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2012-03-12
 *
 */

/*
 * Copyright (C) 2012-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/semaphore.h>
#include <semaphore.h>
#include <libc/allocator.h>

/* libc includes */
#include <errno.h>
#include <time.h>

/* libc-internal includes */
#include <internal/monitor.h>
#include <internal/errno.h>
#include <internal/types.h>
#include <internal/time.h>
#include <internal/init.h>

using namespace Libc;


static Monitor *_monitor_ptr;


void Libc::init_semaphore_support(Monitor &monitor)
{
	_monitor_ptr = &monitor;
}


extern "C" {

	/*
	 * This class is named 'struct sem' because the 'sem_t' type is
	 * defined as 'struct sem*' in 'semaphore.h'
	 */
	struct sem
	{
		int      _count;
		unsigned _applicants { 0 };
		Lock     _data_mutex;
		Lock     _monitor_mutex;

		struct Missing_call_of_init_pthread_support : Exception { };

		struct Applicant
		{
			sem &s;

			Applicant(sem &s) : s(s)
			{
				Lock::Guard lock_guard(s._data_mutex);
				++s._applicants;
			}

			~Applicant()
			{
				Lock::Guard lock_guard(s._data_mutex);
				--s._applicants;
			}
		};

		Monitor & _monitor()
		{
			if (!_monitor_ptr)
				throw Missing_call_of_init_pthread_support();
			return *_monitor_ptr;
		}

		sem(int value) : _count(value) { }

		int trydown()
		{
			Lock::Guard lock_guard(_data_mutex);
			
			if (_count > 0) {
				_count--;
				return 0;
			}

			return EBUSY;
		}

		int down()
		{
			Lock::Guard monitor_guard(_monitor_mutex);

			/* fast path without contention */
			if (trydown() == 0)
				return 0;

			{
				Applicant guard { *this };

				auto fn = [&] { return trydown() == 0; };

				(void)_monitor().monitor(_monitor_mutex, fn);
			}

			return 0;
		}

		int down_timed(timespec const &abs_timeout)
		{
			Lock::Guard monitor_guard(_monitor_mutex);

			/* fast path without wait - does not check abstimeout according to spec */
			if (trydown() == 0)
				return 0;

			timespec abs_now;
			clock_gettime(CLOCK_REALTIME, &abs_now);

			uint64_t const timeout_ms = calculate_relative_timeout_ms(abs_now, abs_timeout);
			if (!timeout_ms)
				return ETIMEDOUT;

			{
				Applicant guard { *this };

				auto fn = [&] { return trydown() == 0; };

				if (_monitor().monitor(_monitor_mutex, fn, timeout_ms))
					return 0;
				else
					return ETIMEDOUT;
			}
		}

		int up()
		{
			Lock::Guard monitor_guard(_monitor_mutex);
			Lock::Guard lock_guard(_data_mutex);

			_count++;

			if (_applicants)
				_monitor().charge_monitors();

			return 0;
		}

		int count()
		{
			return _count;
		}
	};


	int sem_close(sem_t *)
	{
		warning(__func__, " not implemented");
		return Errno(ENOSYS);
	}


	int sem_destroy(sem_t *sem)
	{
		Libc::Allocator alloc { };
		destroy(alloc, *sem);
		return 0;
	}


	int sem_getvalue(sem_t * __restrict sem, int * __restrict sval)
	{
		*sval = (*sem)->count();
		return 0;
	}


	int sem_init(sem_t *sem, int pshared, unsigned int value)
	{
		Libc::Allocator alloc { };
		*sem = new (alloc) struct sem(value);
		return 0;
	}


	sem_t *sem_open(const char *, int, ...)
	{
		warning(__func__, " not implemented");
		return 0;
	}


	int sem_post(sem_t *sem)
	{
		if (int res = (*sem)->up())
			return Errno(res);

		return 0;
	}


	int sem_timedwait(sem_t * __restrict sem, const struct timespec * __restrict abstime)
	{
		/* abstime must be non-null according to the spec */
		if (int res = (*sem)->down_timed(*abstime))
			return Errno(res);

		return 0;
	}


	int sem_trywait(sem_t *sem)
	{
		if (int res = (*sem)->trydown())
			return Errno(res);

		return 0;
	}


	int sem_unlink(const char *)
	{
		warning(__func__, " not implemented");
		return Errno(ENOSYS);
	}


	int sem_wait(sem_t *sem)
	{
		if (int res = (*sem)->down())
			return Errno(res);

		return 0;
	}

}
