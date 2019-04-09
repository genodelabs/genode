/*
 * \brief  Time information
 * \author Josef Soentgen
 * \date   2019-04-09
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__TIME_INFO_H_
#define _NOUX__TIME_INFO_H_

/* Genode includes */
#include <util/string.h>
#include <util/xml_node.h>
#include <rtc_session/connection.h>

/* Noux includes */
#include <noux_session/sysio.h>

namespace Noux {
	class Time_info;
	using namespace Genode;
}


class Noux::Time_info : Noncopyable
{
	private:

		Constructible<Rtc::Connection> _rtc { };

		Genode::int64_t _initial_time { 0 };

		static bool _leap(unsigned year)
		{
			return ((year % 4) == 0
				&& ((year % 100) != 0 || (year % 400) == 0));
		}

		/**
		 * Convert RTC timestamp to UNIX epoch (UTC)
		 */
		static Genode::int64_t _convert(Rtc::Timestamp const &ts)
		{
			if (ts.year < 1970) { return 0; }

			/*
			 * Seconds per year lookup table
			 */
			static constexpr unsigned _secs_per_year[2] = {
				365 * 86400, 366 * 86400,
			};

			/*
			 * Seconds per month lookup table
			 */
			static constexpr unsigned _sec_per_month[13] = {
				  0 * 86400,
				 31 * 86400, 28 * 86400, 31 * 86400, 30 * 86400,
				 31 * 86400, 30 * 86400, 31 * 86400, 31 * 86400,
				 30 * 86400, 31 * 86400, 30 * 86400, 31 * 86400
			};

			Genode::int64_t time = 0;

			for (unsigned i = 1970; i < ts.year; i++) {
				/* abuse bool conversion for seconds look up */
				time += _secs_per_year[(int)_leap(i)];
			}

			for (unsigned i = 1; i < ts.month; i++) {
				time += _sec_per_month[i];
			}
			time += _leap(ts.year) * 86400LL;

			time += 86400LL * (ts.day-1);
			time +=  3600LL * ts.hour;
			time +=    60LL * ts.minute;
			time +=           ts.second;

			return time;
		}

	public:

		Time_info(Env &env, Xml_node config)
		{
			/* only try to establish the connection on demand */
			bool const rtc = config.attribute_value("rtc", false);
			if (!rtc) { return; }

			_rtc.construct(env);
			_initial_time = _convert(_rtc->current_time());
		}

		Genode::int64_t initial_time() const { return _initial_time; }
};

#endif /* _NOUX__TIME_INFO_H_ */
