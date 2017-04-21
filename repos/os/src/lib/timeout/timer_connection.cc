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


void Timer::Connection::_update_interpolation_quality(unsigned long min_factor,
                                                      unsigned long max_factor)
{
	/*
	 * If the factor gets adapted less than 12.5%, we raise the
	 * interpolation-quality value. Otherwise, we reset it to zero.
	 * We can safely do the shift on the factor as it should be
	 * at least of value 1 << TS_TO_US_RATIO_SHIFT.
	 */
	if ((max_factor - min_factor) < (max_factor >> 3)) {
		if (_interpolation_quality < MAX_INTERPOLATION_QUALITY) {
			_interpolation_quality++; }
	} else if (_interpolation_quality) {
		_interpolation_quality = 0;
	}
}


unsigned long Timer::Connection::_ts_to_us_ratio(Timestamp     ts,
                                                 unsigned long us)
{
	/*
	 * If the timestamp difference is to big to do the following
	 * factor calculation without an overflow, scale both timestamp
	 * and time difference down equally. This should neither happen
	 * often nor have much effect on the resulting factor.
	 */
	while (ts > MAX_TS) {
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
	Timestamp     const result    = (ts << TS_TO_US_RATIO_SHIFT) / us;
	unsigned long const result_ul = (unsigned long)result;
	if (result == result_ul) {
		return result_ul; }

	warning("Timestamp-to-time ratio too big");
	return ~0UL;
}


void Timer::Connection::_update_real_time()
{
	Lock_guard<Lock> lock_guard(_real_time_lock);

	Timestamp     ts      = 0UL;
	unsigned long ms      = 0UL;
	unsigned long us_diff = ~0UL;

	for (unsigned remote_time_trials = 0;
	     remote_time_trials < MAX_REMOTE_TIME_TRIALS;
	     remote_time_trials++)
	{
		/* determine time and timestamp difference since the last call */
		Timestamp     volatile new_ts = _timestamp();
		unsigned long volatile new_ms = elapsed_ms();

		if (_interpolation_quality < MAX_INTERPOLATION_QUALITY) {
			ms = new_ms;
			ts = new_ts;
			break;
		}

		Timestamp     const ts_diff     = _timestamp() - new_ts;
		unsigned long const new_us_diff = _ts_to_us_ratio(ts_diff,
		                                                  _us_to_ts_factor);

		/* remember results only if the latency was better than last time */
		if (new_us_diff < us_diff) {
			ms = new_ms;
			ts = new_ts;

			if (us_diff < MAX_REMOTE_TIME_LATENCY_US) {
				break;
			}
		}
	}

	unsigned long const ms_diff = ms - _ms;
	Timestamp     const ts_diff = ts - _ts;

	/* update real time and values for next difference calculation */
	_ms         = ms;
	_ts         = ts;
	_real_time += Milliseconds(ms_diff);

	unsigned long const new_factor = _ts_to_us_ratio(ts_diff, ms_diff * 1000UL);
	unsigned long const old_factor = _us_to_ts_factor;

	/* update interpolation-quality value */
	if (old_factor > new_factor) { _update_interpolation_quality(new_factor, old_factor); }
	else                         { _update_interpolation_quality(old_factor, new_factor); }

	_us_to_ts_factor = new_factor;
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
	if (_interpolated_time < interpolated_time) {
		_interpolated_time = interpolated_time; }

	return _interpolated_time;
}


void Timer::Connection::_handle_timeout()
{
	unsigned long const ms = elapsed_ms();
	if (ms - _ms > REAL_TIME_UPDATE_PERIOD_US / 1000UL) {
		_update_real_time();
	}
	if (_handler) {
		_handler->handle_timeout(Duration(Milliseconds(ms)));
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


Duration Timer::Connection::curr_time()
{
	_enable_modern_mode();

	Reconstructible<Lock_guard<Lock> > lock_guard(_real_time_lock);
	Duration                           interpolated_time(_real_time);

	/*
	 * Interpolate with timestamps only if the factor value
	 * remained stable for some time. If we would interpolate with
	 * a yet unstable factor, there's an increased risk that the
	 * interpolated time falsely reaches an enourmous level. Then
	 * the value would stand still for quite some time because we
	 * can't let it jump back to a more realistic level. This
	 * would also eliminate updates of the real time as the
	 * timeout scheduler that manages our update timeout also
	 * uses this function.
	 */
	if (_interpolation_quality == MAX_INTERPOLATION_QUALITY) {

		/* locally buffer real-time related members */
		unsigned long const ts              = _ts;
		unsigned long const us_to_ts_factor = _us_to_ts_factor;

		lock_guard.destruct();

		/* get time difference since the last real time update */
		Timestamp     const ts_diff = _timestamp() - ts;
		unsigned long const us_diff = _ts_to_us_ratio(ts_diff, us_to_ts_factor);

		interpolated_time += Microseconds(us_diff);

	} else {

		/*
		 * Use remote timer instead of timestamps
		 */
		interpolated_time += Milliseconds(elapsed_ms() - _ms);

		lock_guard.destruct();
	}
	return _update_interpolated_time(interpolated_time);
}


void Timer::Connection::_enable_modern_mode()
{
	if (_mode == MODERN) {
		return;
	}
	_mode = MODERN;
	_sigh(_signal_handler);
	_scheduler._enable();
}


Timer::Connection::Connection(Genode::Env &env, char const *label)
:
	Genode::Connection<Session>(env, session(env.parent(),
	                            "ram_quota=10K, cap_quota=%u, label=\"%s\"",
	                            CAP_QUOTA, label)),
	Session_client(cap()),
	_signal_handler(env.ep(), *this, &Connection::_handle_timeout)
{
	/* register default signal handler */
	Session_client::sigh(_default_sigh_cap);
}


Timer::Connection::Connection()
:
	Genode::Connection<Session>(session("ram_quota=10K")),
	Session_client(cap()),
	_signal_handler(internal_env().ep(), *this, &Connection::_handle_timeout)
{
	/* register default signal handler */
	Session_client::sigh(_default_sigh_cap);
}


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
