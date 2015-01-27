/*
 * \brief  Test for RTC driver
 * \author Christian Helmuth
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>


int main(int argc, char **argv)
{
	Genode::printf("--- RTC test started ---\n");

	/* open sessions */
	static Rtc::Connection   rtc;
	static Timer::Connection timer;

	for (unsigned i = 0; i < 4; ++i) {
		Rtc::Timestamp now = rtc.current_time();

		Genode::printf("RTC: %u-%02u-%02u %02u:%02u:%02u\n",
		               now.year, now.month, now.day,
		               now.hour, now.minute, now.second);

		timer.msleep(1000);
	}

	Genode::printf("--- RTC test finished ---\n");

	return 0;
}
