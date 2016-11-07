/*
 * \brief  Multiplexing one time source amongst different timeouts
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__TIMEOUT_H_
#define _OS__TIMEOUT_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <os/time_source.h>
#include <os/alarm.h>

namespace Genode {

	class Timeout_scheduler;
	class Timeout;
	class Alarm_timeout_scheduler;
	template <typename> class Periodic_timeout;
	template <typename> class One_shot_timeout;
}


/**
 * Interface of a time-source multiplexer
 */
struct Genode::Timeout_scheduler
{
	using Microseconds = Time_source::Microseconds;

	/**
	 *  Read out the now time of the scheduler
	 */
	virtual Microseconds curr_time() const = 0;

	/**
	 * Add a one-shot timeout to the schedule
	 *
	 * \param timeout   timeout callback object
	 * \param duration  timeout trigger delay
	 */
	virtual void schedule_one_shot(Timeout &timeout, Microseconds duration) = 0;

	/**
	 * Add a periodic timeout to the schedule
	 *
	 * \param timeout   timeout callback object
	 * \param duration  timeout trigger period
	 */
	virtual void schedule_periodic(Timeout &timeout, Microseconds duration) = 0;

	/**
	 * Remove timeout from the scheduler
	 *
	 * \param timeout  corresponding timeout callback object
	 */
	virtual void discard(Timeout &timeout) = 0;
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
			using Microseconds = Time_source::Microseconds;

			virtual void handle_timeout(Microseconds curr_time) = 0;
		};

	private:

		using Microseconds = Time_source::Microseconds;


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

		~Timeout() { _alarm.timeout_scheduler.discard(*this); }

		void schedule_periodic(Microseconds duration, Handler &handler);

		void schedule_one_shot(Microseconds duration, Handler &handler);
};


/**
 * Periodic timeout that is linked to a custom handler, starts when constructed
 */
template <typename HANDLER>
struct Genode::Periodic_timeout : private Noncopyable
{
	public:

		using Microseconds = Timeout_scheduler::Microseconds;

	private:

		typedef void (HANDLER::*Handler_method)(Microseconds);

		Timeout _timeout;

		struct Handler : Timeout::Handler
		{
			HANDLER              &object;
			Handler_method const  method;

			Handler(HANDLER &object, Handler_method method)
			: object(object), method(method) { }


			/**********************
			 ** Timeout::Handler **
			 **********************/

			void handle_timeout(Microseconds curr_time) override {
				(object.*method)(curr_time); }

		} _handler;

	public:

		Periodic_timeout(Timeout_scheduler &timeout_scheduler,
		                 HANDLER           &object,
		                 Handler_method     method,
		                 Microseconds       duration)
		:
			_timeout(timeout_scheduler), _handler(object, method)
		{
			_timeout.schedule_periodic(duration, _handler);
		}
};


/**
 * One-shot timeout that is linked to a custom handler, started manually
 */
template <typename HANDLER>
class Genode::One_shot_timeout : private Noncopyable
{
	private:

		using Microseconds = Timeout_scheduler::Microseconds;

		typedef void (HANDLER::*Handler_method)(Microseconds);

		Timeout _timeout;

		struct Handler : Timeout::Handler
		{
			HANDLER              &object;
			Handler_method const  method;

			Handler(HANDLER &object, Handler_method method)
			: object(object), method(method) { }


			/**********************
			 ** Timeout::Handler **
			 **********************/

			void handle_timeout(Microseconds curr_time) override {
				(object.*method)(curr_time); }

		} _handler;

	public:

		One_shot_timeout(Timeout_scheduler &timeout_scheduler,
		                 HANDLER           &object,
		                 Handler_method     method)
		: _timeout(timeout_scheduler), _handler(object, method) { }

		void start(Microseconds duration) {
			_timeout.schedule_one_shot(duration, _handler); }
};


/**
 * Timeout-scheduler implementation using the Alarm framework
 */
class Genode::Alarm_timeout_scheduler : private Noncopyable,
                                        public  Timeout_scheduler,
                                        public  Time_source::Timeout_handler
{
	private:

		Time_source     &_time_source;
		Alarm_scheduler  _alarm_scheduler;


		/**********************************
		 ** Time_source::Timeout_handler **
		 **********************************/

		void handle_timeout(Microseconds curr_time) override;

	public:

		Alarm_timeout_scheduler(Time_source &time_source);


		/***********************
		 ** Timeout_scheduler **
		 ***********************/

		void schedule_one_shot(Timeout &timeout, Microseconds duration) override;
		void schedule_periodic(Timeout &timeout, Microseconds duration) override;

		Microseconds curr_time() const override {
			return _time_source.curr_time(); }

		void discard(Timeout &timeout) override {
			_alarm_scheduler.discard(&timeout._alarm); }
};

#endif /* _OS__TIMEOUT_H_ */
