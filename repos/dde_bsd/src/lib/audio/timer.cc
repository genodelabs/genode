/*
 * \brief  Signal context for timer events
 * \author Josef Soentgen
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/sleep.h>
#include <base/tslab.h>
#include <timer_session/connection.h>

/* local includes */
#include <list.h>
#include <bsd.h>
#include <bsd_emul.h>
#include <scheduler.h>


namespace Bsd {
	class Timer;
}

/**
 * Bsd::Timer
 */
class Bsd::Timer
{
	public:

		struct Timeout
		{
			/* managed kernel timeout */
			timeout &_to;

			/* absolute time in microseconds, used for trigger check */
			Genode::uint64_t  _abs_expires;

			bool _scheduled;

			Timeout(timeout &to, Genode::uint64_t expires)
			: _to { to }, _abs_expires { expires }, _scheduled { false } { }

			void schedule(Genode::uint64_t expires)
			{
				_abs_expires = expires;
				_scheduled = true;
			}

			void discard()
			{
				_scheduled = false;
			}

			void execute()
			{
				_to.fn(_to.arg);
			}

			bool expired(Genode::uint64_t microseconds) const
			{
				return _abs_expires <= microseconds;
			}

			bool scheduled() const
			{
				return _scheduled;
			}

			bool matches(timeout &to) const
			{
				return &_to == &to;
			}
		};

	private:

		/*
		 * Use timer session for delay handling because we must prevent
		 * the calling task and thereby the EP from handling signals.
		 * Otherwise the interrupt task could be executed behind the
		 * suspended task, which leads to problems in the contrib source.
		 */
		::Timer::Connection _delay_timer;

		::Timer::Connection _timer;
		Genode::uint64_t    _microseconds;

		void _update_microseconds()
		{
			_microseconds = _timer.curr_time().trunc_to_plain_us().value;
		}

		Bsd::Task *_sleep_task;

		Bsd::Task _timer_task;

		void _handle_timers(Genode::Duration)
		{
			_update_microseconds();

			_timer_task.unblock();
			Bsd::scheduler().schedule();
		}

		/*
		 * The head of the timeout queue is scheduled via the one shot
		 * timer. If the head changes the currently pending the one shot
		 * timer must be rescheduled.
		 */
		::Timer::One_shot_timeout<Timer> _timers_one_shot;

		/*
		 * For now the timer "queue" is populated by exactly one timer.
		 */
		Genode::Constructible<Timeout> _timeout { };

	public:

		/**
		 * Constructor
		 */
		Timer(Genode::Env &env)
		:
			_delay_timer(env),
			_timer(env),
			_microseconds(_timer.curr_time().trunc_to_plain_us().value),
			_sleep_task(nullptr),
			_timer_task(Timer::run_timer, this, "timer", Bsd::Task::PRIORITY_2,
			            Bsd::scheduler(), 1024 * sizeof(Genode::addr_t)),
			_timers_one_shot(_timer, *this, &Timer::_handle_timers)
		{ }

		static void run_timer(void *p)
		{
			Timer &timer = *static_cast<Timer*>(p);

			while (true) {
				Bsd::scheduler().current()->block_and_schedule();

				timer.execute_timeouts();
			}
		}

		void execute_timeouts()
		{
			if (!_timeout.constructed()) {
				return;
			}

			if (!_timeout->expired(_microseconds)) {
				return;
			}

			_timeout->execute();
		}

		/**
		 * Initialize timeout
		 */
		void timeout_set(struct timeout &to)
		{
			if (_timeout.constructed()) {
				Genode::warning("timeout already constructed");
				return;
			}

			_timeout.construct(to, 0);
		}

		/**
		 * Add timeout to queue
		 */
		int timeout_add_msec(struct timeout &to, int msec)
		{
			if (!_timeout.constructed()) {
				return -1;
			}

			if (!_timeout->matches(to)) {
				return -1;
			}

			_update_microseconds();

			bool const queued = _timeout->scheduled();

			Genode::uint64_t const expires     = msec * 1000;
			Genode::uint64_t const abs_expires = _microseconds + expires;
			_timeout->schedule(abs_expires);
			_timers_one_shot.schedule(Genode::Microseconds { expires });

			return queued ? 1 : 0;
		}

		/**
		 * Remove timeout from queue
		 */
		int timeout_del(struct timeout &to)
		{
			if (!_timeout.constructed()) {
				return -1;
			}

			if (!_timeout->matches(to)) {
				return -1;
			}

			_timers_one_shot.discard();

			bool const queued = _timeout->scheduled();
			_timeout->discard();

			_timeout.destruct();

			return queued ? 1 : 0;
		}

		/**
		 * Update time counter
		 */
		void update_time()
		{
			_update_microseconds();
		}

		/**
		 * Return current microseconds
		 */
		Genode::uint64_t microseconds() const
		{
			return _microseconds;
		}

		/**
		 * Block until delay is reached
		 */
		void delay(Genode::uint64_t us)
		{
			_delay_timer.usleep(us);
		}

		/**
		 * Return pointer for currently sleeping task
		 */
		Bsd::Task *sleep_task() const
		{
			return _sleep_task;
		}

		/**
		 * Set sleep task
		 *
		 * If the argment is 'nullptr' the task is reset.
		 */
		void sleep_task(Bsd::Task *task)
		{
			_sleep_task = task;
		}
};


static Bsd::Timer *_bsd_timer;


void Bsd::timer_init(Genode::Env &env)
{
	static Bsd::Timer bsd_timer(env);
	_bsd_timer = &bsd_timer;
}


void Bsd::update_time()
{
	_bsd_timer->update_time();
}


/*****************
 ** sys/systm.h **
 *****************/

extern "C" int msleep(const volatile void *ident, struct mutex *mtx,
                      int priority, const char *wmesg, int timo)
{
	Bsd::Task *sleep_task = _bsd_timer->sleep_task();

	if (sleep_task) {
		Genode::error("sleep_task is not null, current task: ",
		              Bsd::scheduler().current()->name());
		Genode::sleep_forever();
	}

	sleep_task = Bsd::scheduler().current();
	_bsd_timer->sleep_task(sleep_task);
	sleep_task->block_and_schedule();

	return 0;
}


extern "C" int
msleep_nsec(const volatile void *ident, struct mutex *mtx, int priority,
			const char *wmesg, uint64_t nsecs)
{
	return msleep(ident, mtx, priority, wmesg, nsecs / 1000000);
}


extern "C" void wakeup(const volatile void *ident)
{
	Bsd::Task *sleep_task = _bsd_timer->sleep_task();

	if (!sleep_task) {
		Genode::error("sleep task is NULL");
		Genode::sleep_forever();
	}

	sleep_task->unblock();
	_bsd_timer->sleep_task(nullptr);
}


/*********************
 ** machine/param.h **
 *********************/

extern "C" void delay(int delay)
{
	_bsd_timer->delay((Genode::uint64_t)delay);
}


/****************
 ** sys/time.h **
 ****************/

void microuptime(struct timeval *tv)
{
	/* always update the time */
	_bsd_timer->update_time();

	if (!tv) { return; }

	Genode::uint64_t const ms = _bsd_timer->microseconds();

	tv->tv_sec  = ms / (1000*1000);
	tv->tv_usec = ms % (1000*1000);
}


/*******************
 ** sys/timeout.h **
 *******************/

void timeout_set(struct timeout *to, void (*fn)(void *), void *arg)
{
	if (!to) {
		return;
	}

	to->fn  = fn;
	to->arg = arg;

	_bsd_timer->timeout_set(*to);
}


int timeout_del(struct timeout *to)
{
	return to ? _bsd_timer->timeout_del(*to) : -1;
}


int
timeout_add_msec(struct timeout *to, int msec)
{
	return to ? _bsd_timer->timeout_add_msec(*to, msec) : -1;
}
