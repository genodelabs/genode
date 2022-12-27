/**
 * \brief  Rump-synchronization primitives
 * \author Sebastian Sumpf
 * \author Christian Prochaska (conditional variables)
 * \date   2014-01-13
 */

/*
 * Copyright (C) 2014-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

extern "C" {
#include <sys/cdefs.h>
}
#include <base/mutex.h>
#include <util/fifo.h>
#include <rump/env.h>
#include <rump/timed_semaphore.h>

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
		Genode::Blockade blockade { };

		void block()   { blockade.block(); }
		void wake_up() { blockade.wakeup(); }
	};

	Genode::Fifo<Applicant> fifo;
	bool                    occupied = false;
	Genode::Mutex           meta_lock;

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
				Genode::Mutex::Guard guard(meta_lock);

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

				fifo.enqueue(applicant);
			}
			applicant.block();
		}
	}

	bool enter()     { return _enter(false); }
	bool try_enter() { return _enter(true); }

	void exit()
	{
		Genode::Mutex::Guard guard(meta_lock);

		occupied = false;

		if (flags & RUMPUSER_MTX_KMUTEX) {
			if (owner == nullptr)
				Genode::error("OWNER not set on KMUTEX exit");
			owner = nullptr;
		}

		fifo.dequeue([] (Applicant &applicant) {
			applicant.wake_up(); });
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
	int64_t tv_sec;  /* seconds */
	long    tv_nsec; /* nanoseconds */
};

enum { S_IN_MS = 1000, S_IN_NS = 1000 * 1000 * 1000 };

static uint64_t timeout_ms(struct timespec currtime,
                           struct timespec abstimeout)
{
	if (currtime.tv_nsec >= S_IN_NS) {
		currtime.tv_sec  += currtime.tv_nsec / S_IN_NS;
		currtime.tv_nsec  = currtime.tv_nsec % S_IN_NS;
	}
	if (abstimeout.tv_nsec >= S_IN_NS) {
		abstimeout.tv_sec  += abstimeout.tv_nsec / S_IN_NS;
		abstimeout.tv_nsec  = abstimeout.tv_nsec % S_IN_NS;
	}

	/* check whether absolute timeout is in the past */
	if (currtime.tv_sec > abstimeout.tv_sec)
		return 0;

	int64_t diff_ms = (abstimeout.tv_sec - currtime.tv_sec) * S_IN_MS;
	int64_t diff_ns = 0;

	if (abstimeout.tv_nsec >= currtime.tv_nsec)
		diff_ns = abstimeout.tv_nsec - currtime.tv_nsec;
	else {
		/* check whether absolute timeout is in the past */
		if (diff_ms == 0)
			return 0;
		diff_ns  = S_IN_NS - currtime.tv_nsec + abstimeout.tv_nsec;
		diff_ms -= S_IN_MS;
	}

	diff_ms += diff_ns / 1000 / 1000;

	/* if there is any diff then let the timeout be at least 1 MS */
	if (diff_ms == 0 && diff_ns != 0)
		return 1;

	return diff_ms;
}


struct Cond
{
	int                     num_waiters;
	int                     num_signallers;
	Genode::Mutex           counter_mutex;
	Timed_semaphore         signal_sem { Rump::env().env(),
	                                     Rump::env().ep_thread(),
	                                     Rump::env().timer() };
	Genode::Semaphore       handshake_sem;

	Cond() : num_waiters(0), num_signallers(0) { }

	int timedwait(struct rumpuser_mtx *mutex,
	              const struct timespec *abstime)
	{
		using namespace Genode;
		int result = 0;
	
		counter_mutex.acquire();
		num_waiters++;
		counter_mutex.release();
	
		mutex->exit();
	
		if (!abstime)
			signal_sem.down();
		else {
			struct timespec currtime;
			rumpuser_clock_gettime(0, &currtime.tv_sec, &currtime.tv_nsec);

			Genode::Microseconds timeout_us =
				Genode::Microseconds(timeout_ms(currtime, *abstime) * 1000);
	
			signal_sem.down(true, timeout_us).with_result(
				[&] (Timed_semaphore::Down_ok) { },
				[&] (Timed_semaphore::Down_timed_out) { result = -2; }
			);
		}
	
		counter_mutex.acquire();
		if (num_signallers > 0) {
			if (result == -2) /* timeout occured */
				signal_sem.down();
			handshake_sem.up();
			--num_signallers;
		}
		num_waiters--;
		counter_mutex.release();
	
		mutex->enter();
	
		return result == -2 ? ETIMEDOUT : 0;
	}

	int wait(struct rumpuser_mtx *mutex)
	{
		return timedwait(mutex, 0);
	}

	int signal()
	{
		counter_mutex.acquire();
		if (num_waiters > num_signallers) {
		  ++num_signallers;
		  signal_sem.up();
		  counter_mutex.release();
		  handshake_sem.down();
		} else
		  counter_mutex.release();
	   return 0;
	}

	int broadcast()
	{
		counter_mutex.acquire();
		if (num_waiters > num_signallers) {
			int still_waiting = num_waiters - num_signallers;
			num_signallers = num_waiters;
			for (int i = 0; i < still_waiting; i++)
				signal_sem.up();
			counter_mutex.release();
			for (int i = 0; i < still_waiting; i++)
				handshake_sem.down();
		} else
			counter_mutex.release();
	
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
	rumpuser_clock_gettime(0, &ts.tv_sec, &ts.tv_nsec);

	cv_unschedule(mtx, &nlocks);

	ts.tv_sec += sec;
	ts.tv_nsec += nsec;
	if (ts.tv_nsec >= S_IN_NS) {
		ts.tv_sec  += ts.tv_nsec / S_IN_NS;
		ts.tv_nsec  = ts.tv_nsec % S_IN_NS;
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
	Genode::Mutex      _inc { };
	Genode::Mutex      _write { };

	int _read;

	Rw_lock() : _lock(1), _read(0) {}

	bool read_lock(bool try_lock)
	{
		Genode::Mutex::Guard guard(_inc);

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
		Genode::Mutex::Guard guard(_inc);
		if (--_read == 0)
			unlock();
	}

	bool lock(bool try_lock)
	{
		Genode::Mutex::Guard guard(_write);
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
		Genode::Mutex::Guard guard(_write);
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

