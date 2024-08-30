/*
 * \brief  Libc interval timer
 * \author Norman Feske
 * \date   2024-08-00
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <sys/time.h>

/* libc-internal includes */
#include <internal/signal.h>
#include <internal/timer.h>
#include <internal/init.h>
#include <internal/errno.h>


static Libc::Timer_accessor *_timer_accessor_ptr;
static Libc::Signal         *_signal_ptr;

void Libc::init_alarm(Timer_accessor &timer_accessor, Signal &signal)
{
	_timer_accessor_ptr = &timer_accessor;
	_signal_ptr         = &signal;
}


namespace Libc { struct Itimer_real; }


struct Libc::Itimer_real : Noncopyable
{
	struct Handler : Timeout_handler
	{
		Signal &_signal;

		virtual void handle_timeout() override { _signal.charge(SIGALRM); }

		Handler(Signal &signal) : _signal(signal) { }

	} _handler;

	Timer_accessor &_timer_accessor;

	Constructible<Timeout> _timeout { };

	void arm_or_disarm(timeval tv)
	{
		Libc::uint64_t const ms = tv.tv_sec*1000 + tv.tv_usec/1000;

		if (ms) {
			_timeout.construct(_timer_accessor, _handler);
			_timeout->start(ms);
		} else {
			_timeout.destruct();
		}
	}

	timeval current()
	{
		if (!_timeout.constructed())
			return { };

		Libc::uint64_t const ms = _timeout->duration_left();

		return { .tv_sec  = long(ms/1000),
		         .tv_usec = long((ms % 1000)*1000) };
	}

	Itimer_real(Timer_accessor &timer_accessor, Signal &signal)
	:
		_handler(signal), _timer_accessor(timer_accessor)
	{ }
};


using namespace Libc;


static Itimer_real &itimer_real()
{
	struct Missing_call_of_init_alarm : Exception { };
	if (!_timer_accessor_ptr || !_signal_ptr)
		throw Missing_call_of_init_alarm();

	static Itimer_real itimer { *_timer_accessor_ptr, *_signal_ptr };
	return itimer;
}


extern "C" int setitimer(int which, const itimerval *new_value, itimerval *old_value)
{
	if (which != ITIMER_REAL) {
		warning("setitimer: timer %d unsupported");
		return Errno(EINVAL);
	}

	if (!new_value)
		return Errno(EFAULT);

	if (new_value->it_interval.tv_sec || new_value->it_interval.tv_usec)
		warning("setitimer: argument 'new_value->it_interval' not handled");

	if (old_value) {
		old_value->it_interval = { };
		old_value->it_value    = itimer_real().current();
	}

	itimer_real().arm_or_disarm(new_value->it_value);

	return 0;
}
