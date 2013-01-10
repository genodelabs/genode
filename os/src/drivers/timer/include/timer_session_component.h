/*
 * \brief  Instance of the timer session interface
 * \author Norman Feske
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
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
	class Irq_dispatcher_component : public Genode::Rpc_object<Irq_dispatcher,
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
	 * Alarm for answering a sleep request
	 */
	class Wake_up_alarm : public Genode::Alarm
	{
		private:

			Genode::Native_capability  _reply_cap;
			Genode::Rpc_entrypoint    *_entrypoint;

		public:

			Wake_up_alarm(Genode::Rpc_entrypoint *ep) : _entrypoint(ep) { }

			/**
			 * Set reply target to wake up when the alarm triggers
			 */
			void reply_cap(Genode::Native_capability reply_cap) {
				_reply_cap = reply_cap; }


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
				_entrypoint->explicit_reply(_reply_cap, 0);

				/* do not re-schedule */
				return 0;
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
				_platform_timer(pt),
				_irq_dispatcher_component(this, pt),
				_irq_dispatcher_cap(ep->manage(&_irq_dispatcher_component))
			{
				_platform_timer->schedule_timeout(0);
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
			Genode::Rpc_entrypoint *_entrypoint;
			Wake_up_alarm           _wake_up_alarm;

		public:

			/**
			 * Constructor
			 */
			Session_component(Timeout_scheduler      *ts,
			                  Genode::Rpc_entrypoint *ep)
			:
				_timeout_scheduler(ts),
				_entrypoint(ep),
				_wake_up_alarm(ep)
			{ }

			/**
			 * Destructor
			 */
			~Session_component()
			{
				_timeout_scheduler->discard(&_wake_up_alarm);
			}


			/*****************************
			 ** Timer session interface **
			 *****************************/

			void msleep(unsigned ms)
			{
				_wake_up_alarm.reply_cap(_entrypoint->reply_dst());
				_timeout_scheduler->schedule_timeout(&_wake_up_alarm, 1000*ms);

				/*
				 * Prevent the server activation from immediately answering the
				 * current call. The caller of 'msleep' will be unblocked via
				 * 'Rpc_entrypoint::explicit_reply' when its timeout alarm
				 * triggers.
				 */
				_entrypoint->omit_reply();
			}
	};
}

#endif
