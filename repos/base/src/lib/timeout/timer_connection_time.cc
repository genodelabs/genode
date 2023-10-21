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
	Mutex::Guard guard(_real_time_mutex);


	/*
	 * Update timestamp, time, and real-time value
	 */

	Timestamp ts         = 0UL;
	uint64_t  us         = 0UL;
	uint64_t  latency_us = ~0UL;

	/*
	 * We retry reading out timestamp plus remote time until the result
	 * fulfills a given latency. If the maximum number of trials is
	 * reached, we take the results that has the lowest latency.
	 */
	for (unsigned remote_time_trials = 0;
	     remote_time_trials < MAX_REMOTE_TIME_TRIALS; ) {

		/* read out the two time values close in succession */
		Timestamp const new_ts = _timestamp();
		uint64_t  const new_us = elapsed_us();

		/* do not proceed until the time difference is at least 1 us */
		if (new_us == _us || new_ts == _ts) { continue; }
		remote_time_trials++;

		/*
		 * If interpolation is not ready, yet we cannot determine a latency
		 * and take the values as they are.
		 */
		if (_interpolation_quality < MAX_INTERPOLATION_QUALITY) {
			us = new_us;
			ts = new_ts;
			break;
		}
		/* determine latency between reading out timestamp and time value */
		Timestamp const ts_diff        = _timestamp() - new_ts;
		uint64_t  const new_latency_us =
			_ts_to_us_ratio(ts_diff, _us_to_ts_factor, _us_to_ts_factor_shift);

		/* remember results if the latency was better than on the last trial */
		if (new_latency_us < latency_us) {
			us = new_us;
			ts = new_ts;
			latency_us = new_latency_us;

			/* take the results if the latency fulfills the given maximum */
			if (latency_us < MAX_REMOTE_TIME_LATENCY_US) {
				break;
			}
		}
	}

	/* determine timestamp and time difference */
	uint64_t const us_diff = us - _us;
	Timestamp      ts_diff = ts - _ts;

	/* overwrite timestamp, time, and real time member */
	_us = us;
	_ts = ts;
	_real_time.add(Microseconds(us_diff));


	/*
	 * Update timestamp-to-time factor and its shift
	 */

	unsigned  factor_shift = _us_to_ts_factor_shift;
	uint64_t  old_factor   = _us_to_ts_factor;
	Timestamp max_ts_diff  = ~(Timestamp)0ULL >> factor_shift;

	struct Factor_update_failed : Genode::Exception { };
	try {
		/* meet the timestamp-difference limit before applying the shift */
		while (ts_diff > max_ts_diff) {

			/* if possible, lower the shift to meet the limitation */
			if (!factor_shift) {

				error("timestamp difference too big, ts_diff=", ts_diff,
				      " max_ts_diff=", max_ts_diff);

				throw Factor_update_failed();
			}
			factor_shift--;
			max_ts_diff = (max_ts_diff << 1) | 1;
			old_factor >>= 1;
		}
		/*
		 * Apply current shift to timestamp difference and try to even
		 * raise the shift successively to get as much precision as possible.
		 */
		uint64_t ts_diff_shifted = ts_diff << factor_shift;
		while (ts_diff_shifted < us_diff << MIN_FACTOR_LOG2) {

			factor_shift++;
			ts_diff_shifted <<= 1;
			old_factor <<= 1;
		}
		/*
		 * The cast to uint64_t does not cause a loss because the timestamp
		 * type cannot be bigger as the factor type. We also took care that
		 * the time difference cannot become null.
		 */
		uint64_t const new_factor =
			(uint64_t)ts_diff_shifted / us_diff;

		/* update interpolation-quality value */
		if (old_factor > new_factor) { _update_interpolation_quality(new_factor, old_factor); }
		else                         { _update_interpolation_quality(old_factor, new_factor); }

		/* overwrite factor and factor-shift member */
		_us_to_ts_factor_shift = factor_shift;
		_us_to_ts_factor       = new_factor;

	} catch (Factor_update_failed) {

		/* disable interpolation */
		_interpolation_quality = 0;
	}
}


Duration Timer::Connection::curr_time()
{
	_switch_to_timeout_framework_mode();

	Reconstructible<Mutex::Guard> mutex_guard(_real_time_mutex);
	Duration                      interpolated_time(_real_time);

	/*
	 * Interpolate with timestamps only if the factor value
	 * remained stable for some time. If we would interpolate with
	 * a yet unstable factor, there's an increased risk that the
	 * interpolated time falsely reaches an enourmous level. Then
	 * the value would stand still for quite some time because we
	 * can't let it jump back to a more realistic level.
	 */
	if (_interpolation_quality == MAX_INTERPOLATION_QUALITY) {

		/* buffer interpolation related members and free the mutex */
		Timestamp const ts                    = _ts;
		uint64_t  const us_to_ts_factor       = _us_to_ts_factor;
		unsigned  const us_to_ts_factor_shift = _us_to_ts_factor_shift;

		mutex_guard.destruct();

		/* interpolate time difference since the last real time update */
		Timestamp const ts_current_cpu = _timestamp();
		Timestamp const ts_diff = (ts_current_cpu < ts) ? 0 : ts_current_cpu - ts;
		uint64_t  const us_diff = _ts_to_us_ratio(ts_diff, us_to_ts_factor,
		                                          us_to_ts_factor_shift);

		interpolated_time.add(Microseconds(us_diff));

	} else {
		Timestamp const us      = (Timestamp)elapsed_us();
		Timestamp const us_diff = (Timestamp)((us < _us) ? 0 : us - _us);

		/* use remote timer instead of timestamps */
		interpolated_time.add(Microseconds(us_diff));

		mutex_guard.destruct();
	}
	return _update_interpolated_time(interpolated_time);
}
