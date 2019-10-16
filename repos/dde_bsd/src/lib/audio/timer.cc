/*
 * \brief  Signal context for timer events
 * \author Josef Soentgen
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
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


static Genode::uint64_t millisecs;


namespace Bsd {
	class Timer;
}

/**
 * Bsd::Timer
 */
class Bsd::Timer
{
	private:

		::Timer::Connection                _timer_conn;
		Genode::Signal_handler<Bsd::Timer> _dispatcher;

		/**
		 * Handle trigger_once signal
		 */
		void _handle()
		{
			Bsd::scheduler().schedule();
		}

	public:

		/**
		 * Constructor
		 */
		Timer(Genode::Env &env)
		:
			_timer_conn(env),
			_dispatcher(env.ep(), *this, &Bsd::Timer::_handle)
		{
			_timer_conn.sigh(_dispatcher);
		}

		/**
		 * Update time counter
		 */
		void update_millisecs()
		{
			millisecs = _timer_conn.elapsed_ms();
		}

		void delay(Genode::uint64_t us)
		{
			_timer_conn.usleep(us);
		}
};


static Bsd::Timer *_bsd_timer;


void Bsd::timer_init(Genode::Env &env)
{
	/* XXX safer way preventing possible nullptr access? */
	static Bsd::Timer bsd_timer(env);
	_bsd_timer = &bsd_timer;

	/* initialize value explicitly */
	millisecs = 0;
}


void Bsd::update_time() {
	_bsd_timer->update_millisecs(); }


static Bsd::Task *_sleep_task;


/*****************
 ** sys/systm.h **
 *****************/

extern "C" int msleep(const volatile void *ident, struct mutex *mtx,
                      int priority, const char *wmesg, int timo)
{
	if (_sleep_task) {
		Genode::error("_sleep_task is not null, current task: ",
		              Bsd::scheduler().current()->name());
		Genode::sleep_forever();
	}

	_sleep_task = Bsd::scheduler().current();
	_sleep_task->block_and_schedule();

	return 0;
}

extern "C" void wakeup(const volatile void *ident)
{
	if (!_sleep_task) {
		Genode::error("sleep task is NULL");
		Genode::sleep_forever();
	}
	_sleep_task->unblock();
	_sleep_task = nullptr;
}


/*********************
 ** machine/param.h **
 *********************/

extern "C" void delay(int delay)
{
	_bsd_timer->delay(delay);
}


/**************** 
 ** sys/time.h **
 ****************/

void microuptime(struct timeval *tv)
{
	_bsd_timer->update_millisecs();

	if (!tv) { return; }

	/*
	 * So far only needed by auich_calibrate, which
	 * reuqires microseconds - switching the Bsd::Timer
	 * implementation over to the new Genode::Timer API
	 * is probably necessary for that to work properly.
	 */
	tv->tv_sec = millisecs / 1000;
	tv->tv_usec = 0;
}
