/*
 * \brief  Test for system RTC server
 * \author Josef Soentgen
 * \date   2019-07-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <os/reporter.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>


using namespace Genode;

struct Main
{
	Env &_env;

	Timer::Connection _timer { _env };
	Rtc::Connection   _rtc   { _env };

	void _set_rtc(Reporter &reporter, Rtc::Timestamp const &ts)
	{
		Reporter::Xml_generator xml(reporter, [&] () {
			xml.attribute("year",   ts.year);
			xml.attribute("month",  ts.month);
			xml.attribute("day",    ts.day);
			xml.attribute("hour",   ts.hour);
			xml.attribute("minute", ts.minute);
			xml.attribute("second", ts.second);
		});
	}

	Signal_handler<Main> _set_sigh {
		_env.ep(), *this, &Main::_handle_set_signal };

	Rtc::Timestamp _ts { };

	bool _sys_rtc_set { false };
	bool _drv_rtc_set { false };

	static bool _equal(Rtc::Timestamp const &ts1,
	                   Rtc::Timestamp const &ts2)
	{
		return ts1.year   == ts2.year
		    && ts1.month  == ts2.month
		    && ts1.day    == ts2.day
		    && ts1.hour   == ts2.hour
		    && ts1.minute == ts2.minute;
	}

	bool _check_rtc(Rtc::Timestamp const &ts1,
	                Rtc::Timestamp const &ts2, bool system)
	{
		Genode::log("Set ", system ? "system" : "driver",
		            " RTC to: '", ts1, "' got: '", ts2, "' (ignoring seconds)");

		if (!_equal(ts1, ts2)) {
			error("updating ", system ? "system" : "driver", " RTC failed");
			_parent_exit(1);
			return false;
		}

		return true;
	}

	void _handle_set_signal()
	{
		Rtc::Timestamp got = _rtc.current_time();

		if (_sys_rtc_set) {
			_sys_rtc_set = false;

			if (!_check_rtc(_ts, got, true)) {
				return;
			}

			_ts.year   = 2018;
			_ts.month  =    2;
			_ts.day    =   17;
			_ts.hour   =   10;
			_ts.minute =   15;
			_ts.second =    3;

			log("Set driver RTC value: ", _ts);

			_drv_rtc_set = true;
			_set_rtc(*_drv_reporter, _ts);

			_timer.msleep(5000);
		} else if (_drv_rtc_set) {
			_drv_rtc_set = false;

			if (!_check_rtc(_ts, got, false)) {
				return;
			}

			_ts.year   = 2019;
			_ts.month  =   12;
			_ts.day    =   17;
			_ts.hour   =   11;
			_ts.minute =   15;
			_ts.second =   22;

			log("Set system RTC value: ", _ts);

			_set_rtc(*_sys_reporter, _ts);
			_timer.msleep(3500);
		} else {
			log("RTC value: ", _ts);

			_parent_exit(0);
		}
	}

	Constructible<Reporter> _sys_reporter { };
	Constructible<Reporter> _drv_reporter { };

	void _parent_exit(int exit_code)
	{
		Genode::log("--- system RTC test finished ---");
		_env.parent().exit(exit_code);
	}

	Main(Genode::Env &env) : _env(env)
	{
		_sys_reporter.construct(_env, "sys_set_rtc");
		_sys_reporter->enabled(true);

		_drv_reporter.construct(_env, "drv_set_rtc");
		_drv_reporter->enabled(true);

		Genode::log("--- system RTC test started ---");

		_rtc.set_sigh(_set_sigh);

		_ts = _rtc.current_time();
		log("Initial RTC value: ", _ts);

		_ts.year   = 2020;
		_ts.month  =    7;
		_ts.day    =   16;
		_ts.hour   =   12;
		_ts.minute =   23;
		_ts.second =    1;

		log("Set system RTC value: ", _ts);

		_sys_rtc_set = true;
		_set_rtc(*_sys_reporter, _ts);

		_timer.msleep(5000);
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
