/*
 * \brief  Libc-internal timer handling
 * \author Norman Feske
 * \date   2019-09-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__TIMER_H_
#define _LIBC__INTERNAL__TIMER_H_

/* Genode includes */
#include <timer_session/connection.h>

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {
	class Timer;
	class Timer_accessor;
	class Timeout;
	class Timeout_handler;
}


struct Libc::Timer
{
	::Timer::Connection _timer;

	Timer(Genode::Env &env) : _timer(env) { }

	Duration curr_time()
	{
		return _timer.curr_time();
	}

	static Microseconds microseconds(uint64_t timeout_ms)
	{
		return Microseconds(1000*timeout_ms);
	}

	static uint64_t max_timeout()
	{
		return ~0UL/1000;
	}
};


/**
 * Interface for obtaining the libc-global timer instance
 *
 * The 'Timer' is instantiated on demand whenever the 'Timer_accessor::timer'
 * method is first called. This way, libc-using components do not depend of a
 * timer connection unless they actually use time-related functionality.
 */
struct Libc::Timer_accessor
{
	virtual Timer &timer() = 0;
};


struct Libc::Timeout_handler
{
	virtual void handle_timeout() = 0;
};


/*
 * TODO curr_time wrapping
 */
struct Libc::Timeout
{
	Timer_accessor                     &_timer_accessor;
	Timeout_handler                    &_handler;
	::Timer::One_shot_timeout<Timeout>  _timeout;

	bool     _expired             = true;
	uint64_t _absolute_timeout_ms = 0;

	void _handle(Duration now)
	{
		_expired             = true;
		_absolute_timeout_ms = 0;
		_handler.handle_timeout();
	}

	Timeout(Timer_accessor &timer_accessor, Timeout_handler &handler)
	:
		_timer_accessor(timer_accessor),
		_handler(handler),
		_timeout(_timer_accessor.timer()._timer, *this, &Timeout::_handle)
	{ }

	void start(uint64_t timeout_ms)
	{
		Milliseconds const now = _timer_accessor.timer().curr_time().trunc_to_plain_ms();

		_expired             = false;
		_absolute_timeout_ms = now.value + timeout_ms;

		_timeout.schedule(_timer_accessor.timer().microseconds(timeout_ms));
	}

	uint64_t duration_left() const
	{
		Milliseconds const now = _timer_accessor.timer().curr_time().trunc_to_plain_ms();

		if (_expired || _absolute_timeout_ms < now.value)
			return 0;

		return _absolute_timeout_ms - now.value;
	}
};

#endif /* _LIBC__INTERNAL__TIMER_H_ */
