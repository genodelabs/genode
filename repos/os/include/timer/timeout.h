/*
 * \brief  Multiplexing one time source amongst different timeouts
 * \author Martin Stein
 * \date   2016-11-04
 *
 * These classes are not meant to be used directly. They merely exist to share
 * the generic parts of timeout-scheduling between the Timer::Connection and the
 * Timer driver. For user-level timeout-scheduling you should use the interface
 * in timer_session/connection.h instead.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMER__TIMEOUT_H_
#define _TIMER__TIMEOUT_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <os/alarm.h>
#include <base/log.h>
#include <os/duration.h>

namespace Genode {

	class Time_source;
	class Timeout_scheduler;
	class Timeout;
	class Alarm_timeout_scheduler;
}

namespace Timer
{
	class Connection;
	class Root_component;
}


/**
 * Interface of a time source that can handle one timeout at a time
 */
struct Genode::Time_source
{
	/**
	 * Interface of a timeout callback
	 */
	struct Timeout_handler
	{
		virtual void handle_timeout(Duration curr_time) = 0;
	};

	/**
	 * Return the current time of the source
	 */
	virtual Duration curr_time() = 0;

	/**
	 * Return the maximum timeout duration that the source can handle
	 */
	virtual Microseconds max_timeout() const = 0;

	/**
	 * Install a timeout, overrides the last timeout if any
	 *
	 * \param duration  timeout duration
	 * \param handler   timeout callback
	 */
	virtual void schedule_timeout(Microseconds     duration,
	                              Timeout_handler &handler) = 0;

	/**
	 * Tell the time source which scheduler to use for its own timeouts
	 *
	 * This method enables a time source for example to synchronize with an
	 * accurate but expensive timer only on a periodic basis while using a
	 * cheaper interpolation in general.
	 */
	virtual void scheduler(Timeout_scheduler &scheduler) { };
};


/**
 * Interface of a time-source multiplexer
 *
 * Beside 'curr_time()', this abstract interface is used by the Timeout
 * implementation only. Users of the timeout framework must schedule and
 * discard timeouts via methods of the timeout.
 */
class Genode::Timeout_scheduler
{
	private:

		friend Timeout;

		/**
		 * Add a one-shot timeout to the schedule
		 *
		 * \param timeout   timeout callback object
		 * \param duration  timeout trigger delay
		 */
		virtual void _schedule_one_shot(Timeout &timeout, Microseconds duration) = 0;

		/**
		 * Add a periodic timeout to the schedule
		 *
		 * \param timeout   timeout callback object
		 * \param duration  timeout trigger period
		 */
		virtual void _schedule_periodic(Timeout &timeout, Microseconds duration) = 0;

		/**
		 * Remove timeout from the scheduler
		 *
		 * \param timeout  corresponding timeout callback object
		 */
		virtual void _discard(Timeout &timeout) = 0;

	public:

		/**
		 *  Read out the now time of the scheduler
		 */
		virtual Duration curr_time() = 0;
};


/**
 * Timeout callback that can be used for both one-shot and periodic timeouts
 *
 * This class should be used only if it is necessary to use one timeout
 * callback for both periodic and one-shot timeouts. This is the case, for
 * example, in a Timer-session server. If this is not the case, the classes
 * Periodic_timeout and One_shot_timeout are the better choice.
 */
class Genode::Timeout : private Noncopyable
{
	friend class Alarm_timeout_scheduler;

	public:

		/**
		 * Interface of a timeout handler
		 */
		struct Handler
		{
			virtual void handle_timeout(Duration curr_time) = 0;
		};

	private:

		struct Alarm : Genode::Alarm
		{
			Timeout_scheduler &timeout_scheduler;
			Handler           *handler = nullptr;
			bool               periodic;

			Alarm(Timeout_scheduler &timeout_scheduler)
			: timeout_scheduler(timeout_scheduler) { }


			/*******************
			 ** Genode::Alarm **
			 *******************/

			bool on_alarm(unsigned) override;

		} _alarm;

	public:

		Timeout(Timeout_scheduler &timeout_scheduler)
		: _alarm(timeout_scheduler) { }

		~Timeout() { discard(); }

		void schedule_periodic(Microseconds duration, Handler &handler);

		void schedule_one_shot(Microseconds duration, Handler &handler);

		void discard();

		bool scheduled() { return _alarm.handler != nullptr; }
};


/**
 * Timeout-scheduler implementation using the Alarm framework
 */
class Genode::Alarm_timeout_scheduler : private Noncopyable,
                                        public  Timeout_scheduler,
                                        public  Time_source::Timeout_handler
{
	friend class Timer::Connection;
	friend class Timer::Root_component;

	private:

		Time_source     &_time_source;
		Alarm_scheduler  _alarm_scheduler;

		void _enable();


		/**********************************
		 ** Time_source::Timeout_handler **
		 **********************************/

		void handle_timeout(Duration curr_time) override;


		/***********************
		 ** Timeout_scheduler **
		 ***********************/

		void _schedule_one_shot(Timeout &timeout, Microseconds duration) override;
		void _schedule_periodic(Timeout &timeout, Microseconds duration) override;

		void _discard(Timeout &timeout) override {
			_alarm_scheduler.discard(&timeout._alarm); }

	public:

		Alarm_timeout_scheduler(Time_source &time_source);


		/***********************
		 ** Timeout_scheduler **
		 ***********************/

		Duration curr_time() override { return _time_source.curr_time(); }
};

#endif /* _TIMER__TIMEOUT_H_ */
