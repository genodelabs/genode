/*
 * \brief   A timer manages a continuous time and timeouts on it
 * \author  Martin Stein
 * \date    2016-03-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <kernel/timer.h>
#include <kernel/configuration.h>
#include <hw/assert.h>

using namespace Kernel;


time_t Timer::timeout_age_us(Timeout const * const timeout) const
{
	time_t const age = (time_t)_time - timeout->_start;
	return _ticks_to_us(age);
}


time_t Timer::timeout_max_us() const
{
	return _ticks_to_us(_max_value());
}


bool Timer::_time_overflow(time_t const duration) const
{
	return duration > ~(time_t)0 - _time;
}


void Timer::set_timeout(Timeout * const timeout, time_t const duration)
{
	/*
	 * Remove timeout if it is already in use. Timeouts may get overridden as
	 * result of an update.
	 */
	if (timeout->_listed) {
		_timeout_list[timeout->_end_period].remove(timeout);
	} else {
		timeout->_listed = true; }

	/* set timeout parameters */
	timeout->_start      = _time;
	timeout->_end        = _time + duration;
	timeout->_end_period = _time_overflow(duration) ? !_time_period :
	                                                   _time_period;

	/*
	 * Insert timeout. Timeouts are ordered ascending according to their end
	 * time to be able to quickly determine the nearest timeout.
	 */
	Genode::List<Timeout> & list = _timeout_list[timeout->_end_period];
	Timeout * t1 = 0;
	for (Timeout * t2 = list.first(); t2 && t2->_end < timeout->_end;
	     t1 = t2, t2 = t2->next()) { }

	list.insert(timeout, t1);
}


void Timer::schedule_timeout()
{
	/* get the timeout with the nearest end time */
	Timeout * timeout = _timeout_list[_time_period].first();
	if (!timeout) {
		timeout = _timeout_list[!_time_period].first();
		assert(timeout);
	}
	/* install timeout at timer hardware */
	time_t const duration = (time_t)timeout->_end - _time;
	_last_timeout_duration = duration;
	_start_one_shot(duration);
}


time_t Timer::update_time()
{
	/* determine how much time has passed */
	time_t const old_value = _last_timeout_duration;
	time_t const new_value = _value();
	time_t const duration  = old_value > new_value ? old_value - new_value : 1;

	/* is this the end of the current period? */
	if (_time_overflow(duration)) {

		/* flush all timeouts of current period */
		Genode::List<Timeout> & list = _timeout_list[_time_period];
		while (true) {
			Timeout * const timeout = list.first();
			if (!timeout) { break; }
			list.remove(timeout);
			timeout->_listed = false;
			timeout->timeout_triggered();
		}
		/* switch to next period */
		_time_period = !_time_period;

	}
	/* update time */
	_time += duration;
	return duration;
}


void Timer::process_timeouts()
{
	/*
	 * Walk through timeouts until the first whose end time is in the future.
	 * Consider only the current periods list as all timeouts of the next
	 * period must be in the future.
	 */
	Genode::List<Timeout> & list = _timeout_list[_time_period];
	while (true) {
		Timeout * const timeout = list.first();
		if (!timeout) { break; }
		if (timeout->_end > _time) { break; }
		list.remove(timeout);
		timeout->_listed = false;
		timeout->timeout_triggered();
	}
}


Timer::Timer(unsigned cpu_id) : _cpu_id(cpu_id), _driver(cpu_id)
{
	/*
	 * The timer frequency should allow a good accuracy on the smallest
	 * timeout syscall value (1 us).
	 */
	assert(_ticks_to_us(1) < 1 || _ticks_to_us(_max_value()) == _max_value());

	/*
	 * The maximum measurable timeout is also the maximum age of a timeout
	 * installed by the timeout syscall. The timeout-age syscall returns a
	 * bogus value for older timeouts. A user that awoke from waiting for a
	 * timeout might not be schedulable in the same super period anymore.
	 * However, if the user can't manage to read the timeout age during the
	 * next super period, it's a bad configuration or the users fault. That
	 * said, the maximum timeout should be at least two times the super
	 * period).
	 */
	assert(_ticks_to_us(_max_value()) > 2 * cpu_quota_us);
}
