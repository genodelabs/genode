/*
 * \brief  Helper for converting Trace::Timestamp to epoch
 * \author Johannes Schlatow
 * \date   2022-05-19
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMESTAMP_CALIBRATOR_H_
#define _TIMESTAMP_CALIBRATOR_H_

/* Genode includes */
#include <trace/timestamp.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>

namespace Trace_recorder {
	using namespace Genode;

	class Timestamp_calibrator;
}


class Trace_recorder::Timestamp_calibrator
{
	private:

		uint64_t         const _frequency_hz;
		uint64_t         const _epoch_start_in_us;
		Trace::Timestamp const _ts_start  { Trace::timestamp() };

		enum : uint64_t {
			USEC_PER_SEC   = 1000ULL * 1000ULL,
			USEC_PER_MIN   = USEC_PER_SEC  * 60,
			USEC_PER_HOUR  = USEC_PER_MIN  * 60,
			USEC_PER_DAY   = USEC_PER_HOUR * 24,
		};

		static uint64_t _day_of_year(Rtc::Timestamp time)
		{
			/* look up table, starts with month=0 */
			unsigned days_until_month[] = { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

			uint64_t result = time.day + days_until_month[time.month];

			/* check for leap year */
			if (time.month >= 3) {
				if ((time.year % 1000) == 0 || ((time.year % 4) == 0 && !(time.year % 100) == 0))
					return result + 1;
			}

			return result;
		}

		uint64_t _timestamp_frequency(Env &env, Timer::Connection &timer)
		{
			using namespace Genode;

			/* try getting tsc frequency from platform info, measure if failed */
			try {
				Attached_rom_dataspace const platform_info (env, "platform_info");
				Xml_node const hardware  = platform_info.xml().sub_node("hardware");
				uint64_t const tsc_freq  = hardware.sub_node("tsc").attribute_value("freq_khz", 0ULL);
				bool     const invariant = hardware.sub_node("tsc").attribute_value("invariant", true);

				if (!invariant)
					error("No invariant TSC available");

				if (tsc_freq)
					return tsc_freq * 1000ULL;
			} catch (...) { }

			warning("Falling back to measured timestamp frequency");
			/* measure frequency using timer */
			Trace::Timestamp start = Trace::timestamp();
			timer.msleep(1000);
			return (Trace::timestamp() - start);
		}

		uint64_t _current_epoch_us(Rtc::Connection &rtc)
		{
			Rtc::Timestamp const current_time { rtc.current_time() };

			// assuming year > 2000 or year == 0
			uint64_t usec_until_y2k    = (30*365 + 30/4) * USEC_PER_DAY;
			uint64_t years_since_y2k   = current_time.year ? current_time.year - 2000 : 0;
			uint64_t days_since_y2k    = years_since_y2k * 365 + years_since_y2k/4 -
			                                                     years_since_y2k/100 +
			                                                     years_since_y2k/1000 +
			                             _day_of_year(current_time);

			return usec_until_y2k +
			       days_since_y2k           * USEC_PER_DAY +
			       current_time.hour        * USEC_PER_HOUR +
			       current_time.minute      * USEC_PER_MIN +
			       current_time.second      * USEC_PER_SEC +
			       current_time.microsecond;
		}

	public:

		Timestamp_calibrator(Env &env, Rtc::Connection &rtc, Timer::Connection &timer)
		: _frequency_hz     (_timestamp_frequency(env, timer)),
		  _epoch_start_in_us(_current_epoch_us(rtc))
		{
			log("Timestamp frequency is ", _frequency_hz, "Hz");
		}

		uint64_t ticks_per_second()  const { return _frequency_hz; }

		uint64_t epoch_from_timestamp_in_us(Trace::Timestamp ts) const
		{
			/* intentionally ignoring timestamp wraparounds */
			uint64_t ts_diff = ts - _ts_start;

			return _epoch_start_in_us + (ts_diff / (ticks_per_second() / USEC_PER_SEC));
		}
};


#endif /* _TIMESTAMP_CALIBRATOR_H_ */
