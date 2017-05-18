/*
 * \brief  Test for RTC driver
 * \author Christian Helmuth
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/env.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>


using namespace Genode;

struct Main
{
	Main(Genode::Env &env)
	{
		Genode::log("--- RTC test started ---");

		/* open sessions */
		Rtc::Connection   rtc[] = { { env }, { env, "with_label" } };
		Timer::Connection timer(env);

		for (unsigned i = 0; i < 4; ++i) {
			Rtc::Timestamp now[] = { rtc[0].current_time(), rtc[1].current_time() };

			for (unsigned j = 0; j < sizeof(rtc)/sizeof(*rtc); ++j)
				log("RTC[", j, "]: ",
				    now[j].year, "-", now[j].month,  "-", now[j].day, " ",
				    now[j].hour, ":", now[j].minute, ":", now[j].second);

			timer.msleep(1000);
		}

		Genode::log("--- RTC test finished ---");

		env.parent().exit(0);
	}
};

void Component::construct(Genode::Env &env) { static Main main(env); }
