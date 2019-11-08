/*
 * \brief  Connection to timer service and timeout scheduler
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <base/internal/globals.h>

using namespace Genode;
using namespace Genode::Trace;


void Timer::Connection::_update_interpolation_quality(uint64_t min_factor,
                                                      uint64_t max_factor)
{
	/*
	 * If the factor gets adapted less than 12.5%, we raise the
	 * interpolation-quality value. Otherwise, we reset it to zero.
	 */
	if ((max_factor - min_factor) < (max_factor >> 3)) {
		if (_interpolation_quality < MAX_INTERPOLATION_QUALITY) {
			_interpolation_quality++; }
	} else if (_interpolation_quality) {
		_interpolation_quality = 0;
	}
}


uint64_t Timer::Connection::_ts_to_us_ratio(Timestamp ts,
                                            uint64_t  us,
                                            unsigned  shift)
{
	/*
	 * If the timestamp difference is to big to do the following
	 * factor calculation without an overflow, scale both timestamp
	 * and time difference down equally. This should neither happen
	 * often nor have much effect on the resulting factor.
	 */
	Timestamp const max_ts = ~(Timestamp)0ULL >> shift;
	while (ts > max_ts) {
		warning("timestamp value too big");
		ts >>= 1;
		us >>= 1;
	}
	if (!us) { us = 1; }
	if (!ts) { ts = 1; }

	/*
	 * To make the result more precise, we scale up the numerator of
	 * the calculation. This upscaling must be considered when using
	 * the result.
	 */
	Timestamp const result    = (ts << shift) / us;
	uint64_t  const result_ul = (uint64_t)result;
	if (result != result_ul) {
		warning("Timestamp-to-time ratio too big");
		return ~0UL;
	}
	return result_ul;
}


Duration Timer::Connection::_update_interpolated_time(Duration &interpolated_time)
{
	/*
	 * The new interpolated time value may be smaller than a
	 * previously interpolated time value (based on an older real time
	 * value and factor). In this case, we don't want the user time to
	 * jump back but to freeze at the higher value until the new
	 * interpolation has caught up.
	 */
	if (_interpolated_time.less_than(interpolated_time)) {
		_interpolated_time = interpolated_time; }

	return _interpolated_time;
}


void Timer::Connection::_handle_timeout()
{
	uint64_t const us = elapsed_us();
	if (us - _us > REAL_TIME_UPDATE_PERIOD_US) {
		_update_real_time();
	}
	if (_handler) {
		_handler->handle_timeout(curr_time());
	}
}


void Timer::Connection::schedule_timeout(Microseconds     duration,
                                         Timeout_handler &handler)
{
	if (duration.value < MIN_TIMEOUT_US)
		duration.value = MIN_TIMEOUT_US;

	if (duration.value > REAL_TIME_UPDATE_PERIOD_US)
		duration.value = REAL_TIME_UPDATE_PERIOD_US;

	_handler = &handler;
	trigger_once(duration.value);
}


void Timer::Connection::_enable_modern_mode()
{
	if (_mode == MODERN) {
		return;
	}
	_mode = MODERN;
	_sigh(_signal_handler);
	_scheduler._enable();

	/* do initial calibration burst to make interpolation available earlier */
	for (unsigned i = 0; i < NR_OF_INITIAL_CALIBRATIONS; i++) {
		_update_real_time();
	}
}


Timer::Connection::Connection(Genode::Env &env, Genode::Entrypoint &ep,
                              char const *label)
:
	Genode::Connection<Session>(env, session(env.parent(),
	                            "ram_quota=10K, cap_quota=%u, label=\"%s\"",
	                            CAP_QUOTA, label)),
	Session_client(cap()),
	_signal_handler(ep, *this, &Connection::_handle_timeout)
{
	/* register default signal handler */
	Session_client::sigh(_default_sigh_cap);
}


Timer::Connection::Connection(Genode::Env &env, char const *label)
: Timer::Connection(env, env.ep(), label) {}


void Timer::Connection::_schedule_one_shot(Timeout &timeout, Microseconds duration)
{
	_enable_modern_mode();
	_scheduler._schedule_one_shot(timeout, duration);
};


void Timer::Connection::_schedule_periodic(Timeout &timeout, Microseconds duration)
{
	_enable_modern_mode();
	_scheduler._schedule_periodic(timeout, duration);
};


void Timer::Connection::_discard(Timeout &timeout)
{
	_enable_modern_mode();
	_scheduler._discard(timeout);
}
