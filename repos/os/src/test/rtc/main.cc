/*
 * \brief  Test for RTC driver
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
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

	Rtc::Connection rtc1 { _env };
	Rtc::Connection rtc2 { _env, "with_label" };

	Signal_handler<Main> _set_sigh {
		_env.ep(), *this, &Main::_handle_set_signal };

	Rtc::Timestamp _ts { };

	void _handle_set_signal()
	{
		Rtc::Timestamp got = rtc1.current_time();

		Genode::log("Set RTC to: '", _ts, "' got: '", got,
		            "' (ignoring seconds)");

		int exit_code = 0;
		if (   _ts.year   != got.year
		    || _ts.month  != got.month
		    || _ts.day    != got.day
		    || _ts.hour   != got.hour
		    || _ts.minute != got.minute) {
			error("updating RTC failed");
			exit_code = 1;
		}

		_parent_exit(exit_code);
	}

	Constructible<Reporter> _reporter { };

	void _test_update(Xml_node const &config)
	{
		try {
			_reporter.construct(_env, "set_rtc");
			_reporter->enabled(true);

			rtc1.set_sigh(_set_sigh);

			Rtc::Timestamp ts = rtc1.current_time();
			ts.year   = config.attribute_value("year",   2069U);
			ts.month  = config.attribute_value("month",  12U);
			ts.day    = config.attribute_value("day",    31U);
			ts.hour   = config.attribute_value("hour",   23U);
			ts.minute = config.attribute_value("minute", 58U);
			ts.second = config.attribute_value("second", 0U);

			_ts = ts;

			Reporter::Xml_generator xml(*_reporter, [&] () {
				xml.attribute("year",   ts.year);
				xml.attribute("month",  ts.month);
				xml.attribute("day",    ts.day);
				xml.attribute("hour",   ts.hour);
				xml.attribute("minute", ts.minute);
				xml.attribute("second", ts.second);
			});

		} catch (...) {
			error("could not test RTC update");
			_parent_exit(1);
		}
	}

	void _parent_exit(int exit_code)
	{
		Genode::log("--- RTC test finished ---");
		_env.parent().exit(exit_code);
	}

	Main(Genode::Env &env) : _env(env)
	{
		Genode::log("--- RTC test started ---");

		/* open sessions */
		Timer::Connection timer(env);

		log("test RTC reading");
		for (unsigned i = 0; i < 4; ++i) {
			Rtc::Timestamp now[] = { rtc1.current_time(), rtc2.current_time() };

			for (unsigned j = 0; j < sizeof(now)/sizeof(now[0]); ++j)
				log("RTC[", j, "]: ", now[j]);

			timer.msleep(1000);
		}

		/* test setting the RTC */
		log("test RTC setting");
		Attached_rom_dataspace config_rom { env, "config" };
		bool const test_update = config_rom.xml().attribute_value("set_rtc", false);
		if (test_update) {
			_test_update(config_rom.xml());
			return;
		}

		_parent_exit(0);
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
