/*
 * \brief  Instance of the timer session interface
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_SESSION_COMPONENT_
#define _TIMER_SESSION_COMPONENT_

/* Genode includes */
#include <util/list.h>
#include <os/alarm.h>
#include <base/rpc_server.h>
#include <timer_session/timer_session.h>

/* local includes */
#include "platform_timer.h"


namespace Timer {

	enum { STACK_SIZE = 32*1024 };


	struct Irq_dispatcher {
		GENODE_RPC(Rpc_do_dispatch, void, do_dispatch);
		GENODE_RPC_INTERFACE(Rpc_do_dispatch);
	};


	/**
	 * Timer interrupt handler
	 *
	 * This class represents a RPC object that gets locally called for each
	 * timer interrupt. It is managed by the same entrypoint as all timer
	 * client components. Because the 'do_dispatch' function is executed in
	 * the same thread context as the dispatch functions of client requests,
	 * we are able to answer those requests from here (by calling the
	 * 'handle()' function of the alarm scheduler).
	 */
	class Irq_dispatcher_component : public Rpc_object<Irq_dispatcher,
	                                                   Irq_dispatcher_component>
	{
		private:

			Genode::Alarm_scheduler *_alarm_scheduler;
			Platform_timer          *_platform_timer;

		public:

			/**
			 * Constructor
			 */
			Irq_dispatcher_component(Genode::Alarm_scheduler *as,
			                         Platform_timer          *pt)
			: _alarm_scheduler(as), _platform_timer(pt) { }


			/******************************
			 ** Irq_dispatcher interface **
			 ******************************/

			void do_dispatch()
			{
				using namespace Genode;

				Alarm::Time now = _platform_timer->curr_time();
				Alarm::Time sleep_time;

				/* trigger timeout alarms */
				_alarm_scheduler->handle(now);

				/* determine duration for next one-shot timer event */
				Alarm::Time deadline;
				if (_alarm_scheduler->next_deadline(&deadline))
					sleep_time = deadline - now;
				else
					sleep_time = _platform_timer->max_timeout();

				if (sleep_time == 0)
					sleep_time = 1;

				_platform_timer->schedule_timeout(sleep_time);
			}
	};


	/**
	 * Alarm for answering an oneshot timeout request
	 */
	class Wake_up_alarm : public Genode::Alarm
	{
		private:

			Signal_context_capability _sigh;
			bool                      _periodic;

		public:

			Wake_up_alarm() : _periodic(false) { }

			void sigh(Signal_context_capability sigh) { _sigh = sigh; }
			void periodic(bool periodic) { _periodic = periodic; }
			bool periodic() { return _periodic; }


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
				Signal_transmitter(_sigh).submit();

				return _periodic;
			}
	};


	class Timeout_scheduler : public Genode::Alarm_scheduler,
	                          Genode::Thread<STACK_SIZE>
	{
		private:

			typedef Genode::Capability<Irq_dispatcher>
			        Irq_dispatcher_capability;

			Platform_timer           *_platform_timer;
			Irq_dispatcher_component  _irq_dispatcher_component;
			Irq_dispatcher_capability _irq_dispatcher_cap;

			/**
			 * Timer-interrupt thread
			 *
			 * This thread blocks for the timer interrupt. For each occuring
			 * interrupt, it performs an local RPC call to the server
			 * activation, which, in turn, processes the scheduled timeouts and
			 * reprograms the platform timer.
			 */
			void entry()
			{
				while (true) {

					_platform_timer->wait_for_timeout(this);

					/*
					 * Call timer irq handler to trigger timeout alarms and
					 * reprogram the platform timer.
					 */
					_irq_dispatcher_cap.call<Irq_dispatcher::Rpc_do_dispatch>();
				}
			}

		public:

			/**
			 * Constructor
			 */
			Timeout_scheduler(Platform_timer *pt, Genode::Rpc_entrypoint *ep)
			:
				Thread("timeout_scheduler"),
				_platform_timer(pt),
				_irq_dispatcher_component(this, pt),
				_irq_dispatcher_cap(ep->manage(&_irq_dispatcher_component))
			{
				_platform_timer->schedule_timeout(0);
				start();
			}

			/**
			 * Called from the '_trigger' function executed by the server activation
			 */
			void schedule_timeout(Wake_up_alarm *alarm, Genode::Alarm::Time timeout)
			{
				Alarm::Time now = _platform_timer->curr_time();
				if (alarm->periodic()) {
					handle(now); /* update '_now' in 'Alarm_scheduler' */
					schedule(alarm, timeout);
				} else schedule_absolute(alarm, now + timeout);

				/* interrupt current 'wait_for_timeout' */
				if (head_timeout(alarm))
					_platform_timer->schedule_timeout(0);
			}

			unsigned long curr_time() const
			{
				return _platform_timer->curr_time();
			}
	};


	/**
	 * Timer session
	 */
	class Session_component : public Rpc_object<Session>,
	                          public List<Session_component>::Element
	{
		private:

			Timeout_scheduler  &_timeout_scheduler;
			Wake_up_alarm       _wake_up_alarm;
			unsigned long const _initial_time;

			void _trigger(unsigned us, bool periodic)
			{
				_wake_up_alarm.periodic(periodic);
				_timeout_scheduler.schedule_timeout(&_wake_up_alarm, us);
			}

		public:

			/**
			 * Constructor
			 */
			Session_component(Timeout_scheduler &ts)
			:
				_timeout_scheduler(ts),
				_initial_time(_timeout_scheduler.curr_time())
			{ }

			/**
			 * Destructor
			 */
			~Session_component()
			{
				_timeout_scheduler.discard(&_wake_up_alarm);
			}


			/*****************************
			 ** Timer session interface **
			 *****************************/

			void trigger_once(unsigned us)
			{
				_trigger(us, false);
			}

			void trigger_periodic(unsigned us)
			{
				_trigger(us, true);
			}

			void sigh(Signal_context_capability sigh)
			{
				_wake_up_alarm.sigh(sigh);
			}

			unsigned long elapsed_ms() const
			{
				unsigned long const now = _timeout_scheduler.curr_time();
				return (now - _initial_time) / 1000;
			}

			void msleep(unsigned) { /* never called at the server side */ }
			void usleep(unsigned) { /* never called at the server side */ }
	};
}

#endif
