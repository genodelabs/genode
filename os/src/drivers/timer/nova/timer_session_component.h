/*
 * \brief  Instance of the timer session interface
 * \author Norman Feske
 * \date   2010-01-30
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_SESSION_COMPONENT_
#define _TIMER_SESSION_COMPONENT_

/* Genode includes */
#include <os/alarm.h>
#include <base/rpc_server.h>
#include <timer_session/capability.h>

/* local includes */
#include "platform_timer.h"


namespace Timer {

	enum { STACK_SIZE = sizeof(Genode::addr_t) * 1024 };

	class Wake_up_alarm : public Genode::Alarm
	{
		private:

			Genode::Cancelable_lock *_barrier;

		public:

			Wake_up_alarm(Genode::Cancelable_lock *barrier)
			: _barrier(barrier) { }


			/*********************
			 ** Alarm interface **
			 *********************/

			/**
			 * Dispatch a wakeup alarm
			 *
			 * This function gets called by the 'Alarm_scheduler' thread.
			 */
			bool on_alarm()
			{
				_barrier->unlock();

				/* do not re-schedule */
				return 0;
			}
	};


	class Timeout_scheduler : public Genode::Alarm_scheduler, Genode::Thread<STACK_SIZE>
	{
		private:

			Platform_timer *_platform_timer;

			/**
			 * Timer-interrupt thread
			 */
			void entry()
			{
				using namespace Genode;

				while (true) {

					_platform_timer->wait_for_timeout(this);

					Alarm::Time now = _platform_timer->curr_time();
					Alarm::Time sleep_time;

					/* trigger timeout alarms */
					handle(now);

					/* determine duration for next one-shot timer event */
					Alarm::Time deadline;
					if (next_deadline(&deadline))
						sleep_time = deadline - now;
					else
						sleep_time = _platform_timer->max_timeout();

					if (sleep_time == 0)
						sleep_time = 1;

					_platform_timer->schedule_timeout(sleep_time);
				}
			}

		public:

			/**
			 * Constructor
			 */
			Timeout_scheduler(Platform_timer *pt, Genode::Rpc_entrypoint *ep)
			: Genode::Thread<STACK_SIZE>("irq"), _platform_timer(pt)
			{
				_platform_timer->schedule_timeout(0);
				PDBG("starting timeout scheduler");
				start();
			}

			/**
			 * Called from the 'msleep' function executed by the server activation
			 */
			void schedule_timeout(Genode::Alarm *alarm, Genode::Alarm::Time timeout)
			{
				Genode::Alarm::Time now = _platform_timer->curr_time();
				schedule_absolute(alarm, now + timeout);

				/* interrupt current 'wait_for_timeout' */
				_platform_timer->schedule_timeout(0);
			}
	};


	/**
	 * Timer session
	 */
	class Session_component : public Genode::Rpc_object<Session>,
	                          public Genode::List<Session_component>::Element
	{
		private:

			Timeout_scheduler      *_timeout_scheduler;
			Genode::Rpc_entrypoint  _entrypoint;
			Session_capability      _session_cap;
			Genode::Cancelable_lock _barrier;
			Wake_up_alarm           _wake_up_alarm;

		public:

			/**
			 * Constructor
			 */
			Session_component(Timeout_scheduler   *ts,
			                  Genode::Cap_session *cap)
			:
				_timeout_scheduler(ts),
				_entrypoint(cap, STACK_SIZE, "timer_session_ep"),
				_session_cap(_entrypoint.manage(this)),
				_barrier(Genode::Cancelable_lock::LOCKED),
				_wake_up_alarm(&_barrier)
			{ }

			/**
			 * Destructor
			 */
			~Session_component()
			{
				_entrypoint.dissolve(this);
				_timeout_scheduler->discard(&_wake_up_alarm);
			}

			/**
			 * Return true if capability belongs to session object
			 */
			bool belongs_to(Genode::Session_capability cap)
			{
				return _entrypoint.obj_by_cap(cap) == this;
			}

			/**
			 * Return session capability
			 */
			Session_capability cap() { return _session_cap; }


			/*****************************
			 ** Timer session interface **
			 *****************************/

			void msleep(unsigned ms)
			{
				_timeout_scheduler->schedule_timeout(&_wake_up_alarm, 1000*ms);

				/*
				 * Prevent the server activation from immediately answering the
				 * current call. We will block until the timeout alarm triggers
				 * and unblocks the semaphore.
				 */
				_barrier.lock();
			}
	};
}

#endif
