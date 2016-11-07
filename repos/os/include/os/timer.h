/*
 * \brief  Multiplexes a timer session amongst different timeouts
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/* Genode includes */
#include <timer_session/timer_session.h>
#include <os/time_source.h>
#include <os/timeout.h>

namespace Genode { class Timer; }


/**
 * Multiplexes a timer session amongst different timeouts
 */
class Genode::Timer : public Timeout_scheduler
{
	private:

		class Time_source : public Genode::Time_source
		{
			private:

				enum { MIN_TIMEOUT_US = 100000 };

				using Signal_handler =
					Genode::Signal_handler<Time_source>;

				::Timer::Session &_session;
				Signal_handler    _signal_handler;
				Timeout_handler  *_handler = nullptr;

				void _handle_timeout()
				{
					if (_handler) {
						_handler->handle_timeout(curr_time()); }
				}

			public:

				Time_source(::Timer::Session &session, Entrypoint &ep)
				:
					_session(session),
					_signal_handler(ep, *this, &Time_source::_handle_timeout)
				{
					_session.sigh(_signal_handler);
				}

				Microseconds curr_time() const {
					return Microseconds(1000ULL * _session.elapsed_ms()); }

				void schedule_timeout(Microseconds     duration,
				                      Timeout_handler &handler)
				{
					if (duration.value < MIN_TIMEOUT_US) {
						duration.value = MIN_TIMEOUT_US; }

					if (duration.value > max_timeout().value) {
						duration.value = max_timeout().value; }

					_handler = &handler;
					_session.trigger_once(duration.value);
				}

				Microseconds max_timeout() const {
					return Microseconds(~0UL); }

		} _time_source;

		Alarm_timeout_scheduler _timeout_scheduler { _time_source };

	public:

		Timer(::Timer::Session &session, Entrypoint &ep)
		: _time_source(session, ep) { }


		/***********************
		 ** Timeout_scheduler **
		 ***********************/

		void schedule_periodic(Timeout &timeout, Microseconds duration) override {
			_timeout_scheduler.schedule_periodic(timeout, duration); }

		void schedule_one_shot(Timeout &timeout, Microseconds duration) override {
			_timeout_scheduler.schedule_one_shot(timeout, duration); }

		Microseconds curr_time() const override {
			return _timeout_scheduler.curr_time(); }

		void discard(Timeout &timeout) override {
			_timeout_scheduler.discard(timeout); }
};

#endif /* _TIMER_H_ */
