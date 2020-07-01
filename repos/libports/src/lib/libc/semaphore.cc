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
#include <internal/errno.h>
#include <internal/init.h>
#include <internal/kernel.h>
#include <internal/monitor.h>
#include <internal/time.h>
#include <internal/types.h>

using namespace Libc;


static Timer_accessor *_timer_accessor_ptr;


void Libc::init_semaphore_support(Timer_accessor &timer_accessor)
{
	_timer_accessor_ptr = &timer_accessor;
}


/*
 * This class is named 'struct sem' because the 'sem_t' type is
 * defined as 'struct sem*' in 'semaphore.h'
 */
struct sem : Genode::Noncopyable
{
	private:

		struct Applicant : Genode::Noncopyable
		{
			Applicant *next { nullptr };

			Libc::Blockade &blockade;

			Applicant(Libc::Blockade &blockade) : blockade(blockade) { }
		};

		Applicant *_applicants { nullptr };
		int        _count;
		Mutex      _data_mutex;

		/* _data_mutex must be hold when calling the following methods */

		void _append_applicant(Applicant *applicant)
		{
			Applicant **tail = &_applicants;

			for (; *tail; tail = &(*tail)->next) ;

			*tail = applicant;
		}

		void _remove_applicant(Applicant *applicant)
		{
			Applicant **a = &_applicants;

			for (; *a && *a != applicant; a = &(*a)->next) ;

			*a = applicant->next;
		}

		void _count_up()
		{
			if (Applicant *next = _applicants) {
				_remove_applicant(next);
				next->blockade.wakeup();
			} else {
				++_count;
			}
		}

		bool _applicant_for_semaphore(Libc::Blockade &blockade)
		{
			Applicant applicant { blockade };

			_append_applicant(&applicant);

			_data_mutex.release();

			blockade.block();

			_data_mutex.acquire();

			if (blockade.woken_up()) {
				return true;
			} else {
				_remove_applicant(&applicant);
				return false;
			}
		}

		struct Missing_call_of_init_semaphore_support : Exception { };

		Timer_accessor & _timer_accessor()
		{
			if (!_timer_accessor_ptr)
				throw Missing_call_of_init_semaphore_support();
			return *_timer_accessor_ptr;
		}

		/**
		 * Enqueue current context as applicant for semaphore
		 *
		 * Return true if down was successful, false on timeout expiration.
		 */
		bool _apply_for_semaphore(Libc::uint64_t timeout_ms)
		{
			if (Libc::Kernel::kernel().main_context()) {
				Main_blockade blockade { timeout_ms };
				return _applicant_for_semaphore(blockade);
			} else {
				Pthread_blockade blockade { _timer_accessor(), timeout_ms };
				return _applicant_for_semaphore(blockade);
			}
		}

		/* unsynchronized try */
		int _try_down()
		{
			if (_count > 0) {
				--_count;
				return 0;
			}

			return EBUSY;
		}

	public:

		sem(int value) : _count(value) { }

		int count() const { return _count; }

		int trydown()
		{
			Mutex::Guard guard(_data_mutex);

			return _try_down();
		}

		int down()
		{
			Mutex::Guard guard(_data_mutex);

			/* fast path */
			if (_try_down() == 0)
				return 0;

			_apply_for_semaphore(0);

			return 0;
		}

		int down_timed(timespec const &abs_timeout)
		{
			Mutex::Guard guard(_data_mutex);

			/* fast path */
			if (_try_down() == 0)
				return 0;

			timespec abs_now;
			clock_gettime(CLOCK_REALTIME, &abs_now);

			Libc::uint64_t const timeout_ms =
				calculate_relative_timeout_ms(abs_now, abs_timeout);
			if (!timeout_ms)
				return ETIMEDOUT;

			if (_apply_for_semaphore(timeout_ms))
				return 0;
			else
				return ETIMEDOUT;
		}

		int up()
		{
			Mutex::Guard guard(_data_mutex);

			_count_up();

			return 0;
		}
};


extern "C" {

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
