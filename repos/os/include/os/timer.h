/*
 * \brief  Multiplexes a timer session amongst different timeouts
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/* Genode includes */
#include <util/reconstructible.h>
#include <base/entrypoint.h>
#include <timer_session/timer_session.h>
#include <os/time_source.h>
#include <os/timeout.h>
#include <trace/timestamp.h>

namespace Genode {

	class Timer;
	class Timer_time_source;
}


/**
 * Implementation helper for 'Timer'
 *
 * \noapi
 */
class Genode::Timer_time_source : public Genode::Time_source
{
	private:

		/*
		 * The higher the factor shift, the more precise is the time
		 * interpolation but the more likely it becomes that an overflow
		 * would occur during calculations. In this case, the timer
		 * down-scales the values live which is avoidable overhead.
		 */
		enum { TS_TO_US_RATIO_SHIFT       = 8 };
		enum { MIN_TIMEOUT_US             = 5000 };
		enum { REAL_TIME_UPDATE_PERIOD_US = 100000 };
		enum { MAX_TS                     = ~(Trace::Timestamp)0ULL >> TS_TO_US_RATIO_SHIFT };
		enum { MAX_INTERPOLATION_QUALITY  = 3 };
		enum { MAX_REMOTE_TIME_LATENCY_US = 500 };
		enum { MAX_REMOTE_TIME_TRIALS     = 5 };

		::Timer::Session                                    &_session;
		Io_signal_handler<Timer_time_source>                 _signal_handler;
		Timeout_handler                                     *_handler = nullptr;
		Constructible<Periodic_timeout<Timer_time_source> >  _real_time_update;

		Lock             _real_time_lock        { Lock::UNLOCKED };
		unsigned long    _ms                    { _session.elapsed_ms() };
		Trace::Timestamp _ts                    { _timestamp() };
		Duration         _real_time             { Milliseconds(_ms) };
		Duration         _interpolated_time     { _real_time };
		unsigned         _interpolation_quality { 0 };
		unsigned long    _us_to_ts_factor       { 1UL << TS_TO_US_RATIO_SHIFT };

		Trace::Timestamp _timestamp();

		void _update_interpolation_quality(unsigned long min_factor,
		                                   unsigned long max_factor);

		unsigned long _ts_to_us_ratio(Trace::Timestamp ts, unsigned long us);

		void _handle_real_time_update(Duration);

		Duration _update_interpolated_time(Duration &interpolated_time);

		void _handle_timeout();

	public:

		Timer_time_source(Entrypoint       &ep,
		                  ::Timer::Session &session);


		/*****************
		 ** Time_source **
		 *****************/

		void schedule_timeout(Microseconds     duration,
		                      Timeout_handler &handler) override;

		Microseconds max_timeout() const override { return Microseconds::max(); }

		Duration curr_time() override;

		void scheduler(Timeout_scheduler &scheduler) override;
};


/**
 * Timer-session based timeout scheduler
 *
 * Multiplexes a timer session amongst different timeouts.
 */
struct Genode::Timer : public Genode::Timer_time_source,
                       public Genode::Alarm_timeout_scheduler
{
	using Alarm_timeout_scheduler::curr_time;

	Timer(Entrypoint &ep, ::Timer::Session &session);
};

#endif /* _TIMER_H_ */
