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
	Main(Genode::Env &env)
	{
		int exit_code = 0;

		Genode::log("--- RTC test started ---");

		/* open sessions */
		Rtc::Connection   rtc[] = { { env }, { env, "with_label" } };
		Timer::Connection timer(env);

		for (unsigned i = 0; i < 4; ++i) {
			Rtc::Timestamp now[] = { rtc[0].current_time(), rtc[1].current_time() };

			for (unsigned j = 0; j < sizeof(rtc)/sizeof(*rtc); ++j)
				log("RTC[", j, "]: ", now[j]);

			timer.msleep(1000);
		}

		/* test setting the RTC */
		Attached_rom_dataspace config_rom { env, "config" };
		bool const test_update = config_rom.xml().attribute_value("set_rtc", false);
		if (test_update) {
			try {
				Reporter reporter { env, "set_rtc" };
				reporter.enabled(true);

				Rtc::Timestamp ts = rtc[0].current_time();
				ts.year   = 2069;
				ts.month  = 12;
				ts.day    = 31;
				ts.hour   = 23;
				ts.minute = 55;
				ts.second =  0;

				Reporter::Xml_generator xml(reporter, [&] () {
					xml.attribute("year",   ts.year);
					xml.attribute("month",  ts.month);
					xml.attribute("day",    ts.day);
					xml.attribute("hour",   ts.hour);
					xml.attribute("minute", ts.minute);
					xml.attribute("second", ts.second);
				});

				/*
				 * Wait a reasonable amount of time for the RTC update
				 * to go through.
				 */
				timer.msleep(3000);

				Rtc::Timestamp got = rtc[0].current_time();

				Genode::log("Set RTC to: '", ts, "' got: '", got,
				            "' (ignoring seconds)");

				if (   ts.year   != got.year
				    || ts.month  != got.month
				    || ts.day    != got.day
				    || ts.hour   != got.hour
				    || ts.minute != got.minute) {
					error("updating RTC failed");
					exit_code = 2;
				}

			} catch (...) {
				error("could not test RTC update");
				exit_code = 1;
			}
		}

		Genode::log("--- RTC test finished ---");

		env.parent().exit(exit_code);
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
