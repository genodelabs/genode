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
#include <util/list.h>
#include <base/duration.h>
#include <base/mutex.h>
#include <util/misc_math.h>
#include <base/blockade.h>

namespace Genode {

	class Time_source;
	class Timeout;
	class Timeout_handler;
	class Timeout_scheduler;
}

namespace Timer {

	class Connection;
	class Root_component;
}


/**
 * Interface of a timeout callback
 */
struct Genode::Timeout_handler : Interface
{
	virtual void handle_timeout(Duration curr_time) = 0;
};


/**
 * Interface of a time source that can handle one timeout at a time
 */
struct Genode::Time_source : Interface
{
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
	virtual void set_timeout(Microseconds     duration,
	                         Timeout_handler &handler) = 0;
};


/**
 * Timeout callback that can be used for both one-shot and periodic timeouts
 *
 * This class should be used only if it is necessary to use one timeout
 * callback for both periodic and one-shot timeouts. This is the case, for
 * example, in a Timer-session server. If this is not the case, the classes
 * Periodic_timeout and One_shot_timeout are the better choice.
 */
class Genode::Timeout : private Noncopyable,
                        public Genode::List<Timeout>::Element
{
	friend class Timeout_scheduler;

	private:

		Mutex                  _mutex               { };
		Timeout_scheduler     &_scheduler;
		Microseconds           _period              { 0 };
		Microseconds           _deadline            { Microseconds { 0 } };
		List_element<Timeout>  _pending_timeouts_le { this };
		Timeout_handler       *_pending_handler     { nullptr };
		Timeout_handler       *_handler             { nullptr };
		bool                   _in_discard_blockade { false };
		Blockade               _discard_blockade    { };

		Timeout(Timeout const &);

		Timeout &operator = (Timeout const &);

	public:

		Timeout(Timeout_scheduler &scheduler);

		Timeout(Timer::Connection &timer_connection);

		~Timeout();

		void schedule_periodic(Microseconds     duration,
		                       Timeout_handler &handler);

		void schedule_one_shot(Microseconds     duration,
		                       Timeout_handler &handler);

		void discard();

		bool scheduled();

		Microseconds deadline() const { return _deadline; }
};


/**
 * Multiplexes one time source amongst different timeouts
 */
class Genode::Timeout_scheduler : private Noncopyable,
                                  public  Timeout_handler
{
	friend class Timer::Connection;
	friend class Timer::Root_component;
	friend class Timeout;

	private:

		static constexpr uint64_t max_sleep_time_us { 60'000'000 };

		Mutex               _mutex              { };
		Time_source        &_time_source;
		Microseconds const  _max_sleep_time     { min(_time_source.max_timeout().value, max_sleep_time_us) };
		List<Timeout>       _timeouts           { };
		Microseconds        _current_time       { 0 };
		bool                _destructor_called  { false };
		Microseconds        _rate_limit_period;
		Microseconds        _rate_limit_deadline;

		void _insert_into_timeouts_list(Timeout &timeout);

		void _set_time_source_timeout();

		void _set_time_source_timeout(uint64_t duration_us);

		void _schedule_timeout(Timeout         &timeout,
		                       Microseconds     duration,
		                       Microseconds     period,
		                       Timeout_handler &handler);

		void _discard_timeout_unsynchronized(Timeout &timeout);

		void _enable();

		void _schedule_one_shot_timeout(Timeout         &timeout,
		                                Microseconds     duration,
                                        Timeout_handler &handler);

		void _schedule_periodic_timeout(Timeout         &timeout,
		                                Microseconds     period,
		                                Timeout_handler &handler);

		void _discard_timeout(Timeout &timeout);

		void _destruct_timeout(Timeout &timeout);

		Timeout_scheduler(Timeout_scheduler const &);

		Timeout_scheduler &operator = (Timeout_scheduler const &);


		/*********************
		 ** Timeout_handler **
		 *********************/

		void handle_timeout(Duration curr_time) override;

	public:

		Timeout_scheduler(Time_source  &time_source,
		                  Microseconds  min_handle_period);

		~Timeout_scheduler();

		Duration curr_time();
};

#endif /* _TIMER__TIMEOUT_H_ */
