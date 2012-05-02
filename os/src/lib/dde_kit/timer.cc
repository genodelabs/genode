/*
 * \brief  Timers and ticks
 * \author Christian Helmuth
 * \date   2008-10-22
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/list.h>
#include <os/alarm.h>
#include <timer_session/connection.h>
#include <base/printf.h>

extern "C" {
#include <dde_kit/timer.h>
}

#include "thread.h"

using namespace Genode;


/************************
 ** Timer tick symbols **
 ************************/

volatile unsigned long dde_kit_timer_ticks;
extern "C" volatile unsigned long jiffies __attribute__ ((alias ("dde_kit_timer_ticks")));


/***************************
 ** Timer thread and tick **
 ***************************/

class Timer_thread : Dde_kit::Thread, public Alarm_scheduler
{
	private:

		unsigned            _period_in_ms;

		void(*_init)(void *);
		void               *_priv;

		Timer::Connection   _timer;
		List<dde_kit_timer> _destroy_list;
		Lock                _destroy_list_lock;

		void _enqueue_destroy(dde_kit_timer *timer)
		{
			Lock::Guard lock_guard(_destroy_list_lock);

			_destroy_list.insert(timer);
		}

		dde_kit_timer * _dequeue_next_destroy()
		{
			Lock::Guard lock_guard(_destroy_list_lock);

			dde_kit_timer *timer = _destroy_list.first();
			if (timer)
				_destroy_list.remove(timer);

			return timer;
		}

	public:

		Timer_thread(unsigned hz, void(*init)(void *), void *priv)
		: Dde_kit::Thread("timer"), _period_in_ms(1000/hz), _init(init), _priv(priv)
		{
			dde_kit_timer_ticks = 0;
			start();
		}

		void entry()
		{
			dde_kit_thread_adopt_myself("timer");

			/* call provided init function */
			if (_init) _init(_priv);

			/* timer tick loop */
			while (true) {
				dde_kit_timer *timer;

				/*
				 * XXX This approach drifts with the execution time of handlers
				 *     and timer destruction.
				 */
				_timer.msleep(_period_in_ms);
				++dde_kit_timer_ticks;

				/* execute all scheduled alarms */
				Alarm_scheduler::handle(dde_kit_timer_ticks);

				/* finish pending alarm object destruction */
				while ((timer = _dequeue_next_destroy()))
					destroy(env()->heap(), timer);
			}
		};

		/**
		 * Schedule timer for destruction (garbage collection)
		 *
		 * \param   timer  timer fo destroy
		 */
		void destroy_timer(dde_kit_timer *timer)
		{
			_enqueue_destroy(timer);
		}
};


static Timer_thread *_timer_thread;


/*************+******
 ** Timer facility **
 ********************/

class dde_kit_timer : public Alarm, public List<dde_kit_timer>::Element
{
	private:

		void (*_handler)(void *);  /* handler function */
		void  *_priv;              /* private handler token */

		bool   _pending;           /* true if timer is pending */

	protected:

		bool on_alarm()
		{
			/* if timer is really pending, call registered handler function */
			if (_pending) {
				_handler(_priv);
				_pending = false;
			}

			/* do not schedule again */
			return false;
		}

	public:

		dde_kit_timer(void (*handler)(void *), void *priv, unsigned long absolute_timeout)
		: _handler(handler), _priv(priv), _pending(true) {
			schedule(absolute_timeout); }

		void schedule(unsigned long absolute_timeout)
		{
			_pending = true;
			_timer_thread->schedule_absolute(this, absolute_timeout);
		}

		bool pending() const { return _pending; }

		/**
		 * Schedule destruction of this timer on next tick
		 *
		 * Note: The timed event scheduler (alarm.h) does not allow to modify
		 *       alarm objects in the on_alarm function. But, drivers do this
		 *       frequently when modifying timer objects on timeout occurence.
		 */
		void destroy()
		{
			_pending = false;
			_timer_thread->destroy_timer(this);
		}
};


extern "C" struct dde_kit_timer *dde_kit_timer_add(void (*fn)(void *), void *priv,
                                                   unsigned long timeout)
{
	try {
		return new (env()->heap()) dde_kit_timer(fn, priv, timeout);
	} catch (...) {
		PERR("timer creation failed");
		return 0;
	}
}


extern "C" void dde_kit_timer_schedule_absolute(struct dde_kit_timer *timer, unsigned long timeout) {
	timer->schedule(timeout); }


extern "C" void dde_kit_timer_del(struct dde_kit_timer *timer)
{
	try {
		timer->destroy();
	} catch (...) { }
}


extern "C" int dde_kit_timer_pending(struct dde_kit_timer *timer) {
	return timer->pending();
}


extern "C" void dde_kit_timer_init(void(*thread_init)(void *), void *priv)
{
	try {
		static Timer_thread t(DDE_KIT_HZ, thread_init, priv);
		_timer_thread = &t;
	} catch (...) {
		PERR("Timer thread creation failed");
	}
}
