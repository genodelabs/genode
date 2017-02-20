/**
 * \brief  Rump-synchronization primitives
 * \author Sebastian Sumpf
 * \author Christian Prochaska (conditional variables)
 * \date   2014-01-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

extern "C" {
#include <sys/cdefs.h>
}
#include <base/lock.h>
#include <util/fifo.h>
#include <os/timed_semaphore.h>
#include <rump/env.h>

#include "sched.h"


/*************
 ** Mutexes **
 *************/

/**
 * Mutex with support for try_enter()
 *
 * The mutex is a binary semaphore based on the implementation of
 * Genode::Semaphore using an applicant FIFO.
 */
struct rumpuser_mtx
{
	struct Applicant : Genode::Fifo<Applicant>::Element
	{
		Genode::Lock lock { Genode::Lock::LOCKED };

		void block()   { lock.lock();   }
		void wake_up() { lock.unlock(); }
	};

	Genode::Fifo<Applicant> fifo;
	bool                    occupied = false;
	Genode::Lock            meta_lock;

	struct lwp *owner = nullptr;
	int         flags;

	rumpuser_mtx(int flags) : flags(flags) { }

	bool _enter(bool try_enter)
	{
		while (true) {
			/*
			 * We need a freshly constructed applicant instance on each loop
			 * iteration to potentially add ourself to the applicant FIFO
			 * again.
			 */
			Applicant applicant;
			{
				Genode::Lock::Guard guard(meta_lock);

				if (!occupied) {
					occupied = true;

					if (flags & RUMPUSER_MTX_KMUTEX) {
						if (owner != nullptr)
							Genode::error("OWNER already set on KMUTEX enter");
						owner = rumpuser_curlwp();
					}

					return true;
				}

				if (try_enter)
					return false;

				fifo.enqueue(&applicant);
			}
			applicant.block();
		}
	}

	bool enter()     { return _enter(false); }
	bool try_enter() { return _enter(true); }

	void exit()
	{
		Genode::Lock::Guard guard(meta_lock);

		occupied = false;

		if (flags & RUMPUSER_MTX_KMUTEX) {
			if (owner == nullptr)
				Genode::error("OWNER not set on KMUTEX exit");
			owner = nullptr;
		}

		if (Applicant *applicant = fifo.dequeue())
			applicant->wake_up();
	}
};


void rumpuser_mutex_init(struct rumpuser_mtx **mtxp, int flags)
{
	*mtxp = new(Rump::env().heap()) rumpuser_mtx(flags);
}


void rumpuser_mutex_owner(struct rumpuser_mtx *mtx, struct lwp **lp)
{
	/* XXX we set the owner in KMUTEX only */
	*lp = mtx->owner;
}


void rumpuser_mutex_enter_nowrap(struct rumpuser_mtx *mtx)
{
	mtx->enter();
}


void rumpuser_mutex_enter(struct rumpuser_mtx *mtx)
{
	if (mtx->flags & RUMPUSER_MTX_SPIN) {
		rumpuser_mutex_enter_nowrap(mtx);
		return;
	}

	if (!mtx->try_enter()) {
		int nlocks;
		rumpkern_unsched(&nlocks, 0);
		mtx->enter();
		rumpkern_sched(nlocks, 0);
	}
}


int rumpuser_mutex_tryenter(struct rumpuser_mtx *mtx)
{
	return mtx->try_enter() ? 0 : 1;
}


void rumpuser_mutex_exit(struct rumpuser_mtx *mtx)
{
	mtx->exit();
}


void rumpuser_mutex_destroy(struct rumpuser_mtx *mtx)
{
	destroy(Rump::env().heap(), mtx);
}


/***************************
 ** Conditional variables **
 ***************************/

struct timespec {
	long   tv_sec;    /* seconds */
	long   tv_nsec;/* nanoseconds */
};


static unsigned long timespec_to_ms(const struct timespec ts)
{
	return (ts.tv_sec * 1000) + (ts.tv_nsec / (1000 * 1000));
}


struct Cond
{
	int                     num_waiters;
	int                     num_signallers;
	Genode::Lock            counter_lock;
	Genode::Timed_semaphore signal_sem;
	Genode::Semaphore       handshake_sem;

	Cond() : num_waiters(0), num_signallers(0) { }

	int timedwait(struct rumpuser_mtx *mutex,
	              const struct timespec *abstime)
	{
		using namespace Genode;
		int result = 0;
		Alarm::Time timeout = 0;
	
		counter_lock.lock();
		num_waiters++;
		counter_lock.unlock();
	
		mutex->exit();
	
		if (!abstime)
			signal_sem.down();
		else {
			struct timespec currtime;
			rumpuser_clock_gettime(0, (int64_t *)&currtime.tv_sec, &currtime.tv_nsec);
			unsigned long abstime_ms = timespec_to_ms(*abstime);
			unsigned long currtime_ms = timespec_to_ms(currtime);
			if (abstime_ms > currtime_ms)
				timeout = abstime_ms - currtime_ms;
			try {
				signal_sem.down(timeout);
			} catch (Timeout_exception) {
				result = -2;
			}
			catch (Nonblocking_exception) {
				result = 0;
			}
		}
	
		counter_lock.lock();
		if (num_signallers > 0) {
			if (result == -2) /* timeout occured */
				signal_sem.down();
			handshake_sem.up();
			--num_signallers;
		}
		num_waiters--;
		counter_lock.unlock();
	
		mutex->enter();
	
		return result == -2 ? ETIMEDOUT : 0;
	}

	int wait(struct rumpuser_mtx *mutex)
	{
		return timedwait(mutex, 0);
	}

	int signal()
	{
		counter_lock.lock();
		if (num_waiters > num_signallers) {
		  ++num_signallers;
		  signal_sem.up();
		  counter_lock.unlock();
		  handshake_sem.down();
		} else
		  counter_lock.unlock();
	   return 0;
	}

	int broadcast()
	{
		counter_lock.lock();
		if (num_waiters > num_signallers) {
			int still_waiting = num_waiters - num_signallers;
			num_signallers = num_waiters;
			for (int i = 0; i < still_waiting; i++)
				signal_sem.up();
			counter_lock.unlock();
			for (int i = 0; i < still_waiting; i++)
				handshake_sem.down();
		} else
			counter_lock.unlock();
	
		return 0;
	}
};


struct rumpuser_cv {
	Cond  cond;
};


void rumpuser_cv_init(struct rumpuser_cv **cv)
{
	*cv = new(Rump::env().heap())  rumpuser_cv();
}


void rumpuser_cv_destroy(struct rumpuser_cv *cv)
{
	destroy(Rump::env().heap(), cv);
}


static void cv_unschedule(struct rumpuser_mtx *mtx, int *nlocks)
{
	rumpkern_unsched(nlocks, mtx);
}


static void cv_reschedule(struct rumpuser_mtx *mtx, int nlocks)
{

	/*
	 * If the cv interlock is a spin mutex, we must first release
	 * the mutex that was reacquired by _pth_cond_wait(),
	 * acquire the CPU context and only then relock the mutex.
	 * This is to preserve resource allocation order so that
	 * we don't deadlock.  Non-spinning mutexes don't have this
	 * problem since they don't use a hold-and-wait approach
	 * to acquiring the mutex wrt the rump kernel CPU context.
	 *
	 * The more optimal solution would be to rework rumpkern_sched()
	 * so that it's possible to tell the scheduler
	 * "if you need to block, drop this lock first", but I'm not
	 * going poking there without some numbers on how often this
	 * path is taken for spin mutexes.
	 */
	if ((mtx->flags & (RUMPUSER_MTX_SPIN | RUMPUSER_MTX_KMUTEX)) ==
	    (RUMPUSER_MTX_SPIN | RUMPUSER_MTX_KMUTEX)) {
		mtx->exit();
		rumpkern_sched(nlocks, mtx);
		rumpuser_mutex_enter_nowrap(mtx);
	} else {
		rumpkern_sched(nlocks, mtx);
	}
}


void rumpuser_cv_wait(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx)
{
	int nlocks;

	cv_unschedule(mtx, &nlocks);
	cv->cond.wait(mtx);
	cv_reschedule(mtx, nlocks);
}


void rumpuser_cv_wait_nowrap(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx)
{
	cv->cond.wait(mtx);
}


int rumpuser_cv_timedwait(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx,
	int64_t sec, int64_t nsec)
{
	struct timespec ts;
	int rv, nlocks;

	/*
	 * Get clock already here, just in case we will be put to sleep
	 * after releasing the kernel context.
	 *
	 * The condition variables should use CLOCK_MONOTONIC, but since
	 * that's not available everywhere, leave it for another day.
	 */
	rumpuser_clock_gettime(0, (int64_t *)&ts.tv_sec, &ts.tv_nsec);

	cv_unschedule(mtx, &nlocks);

	ts.tv_sec += sec;
	ts.tv_nsec += nsec;
	if (ts.tv_nsec >= 1000*1000*1000) {
		ts.tv_sec++;
		ts.tv_nsec -= 1000*1000*1000;
	}

	rv = cv->cond.timedwait(mtx, &ts);

	cv_reschedule(mtx, nlocks);

	return rv;
}


void rumpuser_cv_signal(struct rumpuser_cv *cv)
{
	cv->cond.signal();
}


void rumpuser_cv_broadcast(struct rumpuser_cv *cv)
{
	cv->cond.broadcast();
}


void rumpuser_cv_has_waiters(struct rumpuser_cv *cv, int *nwaiters)
{
	*nwaiters = cv->cond.num_waiters;
}


/*********************
 ** Read/write lock **
 *********************/

struct Rw_lock {

	Genode::Semaphore  _lock;
	Genode::Lock       _inc;
	Genode::Lock       _write;

	int _read;

	Rw_lock() : _lock(1), _read(0) {}

	bool read_lock(bool try_lock)
	{
		Genode::Lock::Guard guard(_inc);

		if (_read > 0) {
			_read++;
			return true;
		}

		bool locked = lock(true);

		if (locked) {
			_read = 1;
			return true;
		}

		if (try_lock)
			return false;

		lock(false);
		_read = 1;
		return true;
	}

	void read_unlock()
	{
		Genode::Lock::Guard guard(_inc);
		if (--_read == 0)
			unlock();
	}

	bool lock(bool try_lock)
	{
		Genode::Lock::Guard guard(_write);
		if (_lock.cnt() > 0) {
			_lock.down();
			return true;
		}

		if (try_lock)
			return false;

		_lock.down();
		return true;
	}

	void unlock()
	{
		Genode::Lock::Guard guard(_write);
		_lock.up();
	}

	int readers() { return _read; }
	int writer()  { return (_lock.cnt() <= 0 && !_read) ? 1 : 0; }
};


struct rumpuser_rw
{
	Rw_lock rw;
};


void rumpuser_rw_init(struct rumpuser_rw **rw)
{
	*rw = new(Rump::env().heap()) rumpuser_rw();
}


void rumpuser_rw_enter(int enum_rumprwlock, struct rumpuser_rw *rw)
{
	int nlocks;
	bool try_lock = true;
	bool locked   = false;


	while (!locked) {

		if (!try_lock)
			rumpkern_unsched(&nlocks, 0);

		if (enum_rumprwlock == RUMPUSER_RW_WRITER)
			locked = rw->rw.lock(try_lock);
		else
			locked = rw->rw.read_lock(try_lock);

		if (!try_lock)
			rumpkern_sched(nlocks, 0);

		try_lock = false;
	}
}


int rumpuser_rw_tryenter(int enum_rumprwlock, struct rumpuser_rw *rw)
{
	bool locked = enum_rumprwlock == RUMPUSER_RW_WRITER ?
	              rw->rw.lock(true) : rw->rw.read_lock(true);

	return locked ? 0 : 1;
}


int rumpuser_rw_tryupgrade(struct rumpuser_rw *rw)
{
	return 1;
}


void rumpuser_rw_downgrade(struct rumpuser_rw *rw)
{
}


void rumpuser_rw_exit(struct rumpuser_rw *rw)
{
	if (rw->rw.readers())
		rw->rw.read_unlock();
	else
		rw->rw.unlock();
}


void rumpuser_rw_held(int enum_rumprwlock, struct rumpuser_rw *rw, int *rv)
{
	*rv = enum_rumprwlock == RUMPUSER_RW_WRITER ? rw->rw.writer() :
	                         rw->rw.readers();
}


void rumpuser_rw_destroy(struct rumpuser_rw *rw)
{
	destroy(Rump::env().heap(), rw);
}

