/*
 * \brief  Signal context for timer events
 * \author Josef Soentgen
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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


static unsigned long millisecs;


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
		Timer(Genode::Entrypoint &ep)
		:
			_dispatcher(ep, *this, &Bsd::Timer::_handle)
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
};


static Bsd::Timer *_bsd_timer;


void Bsd::timer_init(Genode::Entrypoint &ep)
{
	/* XXX safer way preventing possible nullptr access? */
	static Bsd::Timer bsd_timer(ep);
	_bsd_timer = &bsd_timer;

	/* initialize value explicitly */
	millisecs = 0UL;
}


void Bsd::update_time() {
	_bsd_timer->update_millisecs(); }


static Timer::Connection _timer;


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
	_sleep_task->unblock();
	_sleep_task = nullptr;
}


/*********************
 ** machine/param.h **
 *********************/

extern "C" void delay(int delay)
{
	_timer.msleep(delay);
}
