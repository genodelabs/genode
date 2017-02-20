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
#include <timer_session/timer_session.h>
#include <os/time_source.h>
#include <os/timeout.h>

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

		enum { MIN_TIMEOUT_US = 5000 };

		using Signal_handler = Genode::Signal_handler<Timer_time_source>;

		::Timer::Session &_session;
		Signal_handler    _signal_handler;
		Timeout_handler  *_handler = nullptr;

		void _handle_timeout()
		{
			if (_handler)
				_handler->handle_timeout(curr_time());
		}

	public:

		Timer_time_source(::Timer::Session &session, Entrypoint &ep)
		:
			_session(session),
			_signal_handler(ep, *this, &Timer_time_source::_handle_timeout)
		{
			_session.sigh(_signal_handler);
		}

		Microseconds curr_time() const {
			return Microseconds(1000UL * _session.elapsed_ms()); }

		void schedule_timeout(Microseconds     duration,
		                      Timeout_handler &handler)
		{
			if (duration.value < MIN_TIMEOUT_US)
				duration.value = MIN_TIMEOUT_US;

			if (duration.value > max_timeout().value)
				duration.value = max_timeout().value;

			_handler = &handler;
			_session.trigger_once(duration.value);
		}

		Microseconds max_timeout() const { return Microseconds::max(); }
};


/**
 * Timer-session based timeout scheduler
 *
 * Multiplexes a timer session amongst different timeouts.
 */
struct Genode::Timer : private Genode::Timer_time_source,
                       public  Genode::Alarm_timeout_scheduler
{
	using Time_source::Microseconds;
	using Alarm_timeout_scheduler::curr_time;

	Timer(::Timer::Session &session, Entrypoint &ep)
	:
		Timer_time_source(session, ep),
		Alarm_timeout_scheduler(*(Time_source*)this)
	{ }
};

#endif /* _TIMER_H_ */
