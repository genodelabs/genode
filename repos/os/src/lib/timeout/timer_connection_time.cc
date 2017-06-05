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
	if (_interpolation_quality == MAX_INTERPOLATION_QUALITY)
	{
		/* locally buffer real-time related members */
		Timestamp     const ts              = _ts;
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
