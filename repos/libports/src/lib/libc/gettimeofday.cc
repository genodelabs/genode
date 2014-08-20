/*
 * \brief  C-library back end
 * \author Norman Feske
 * \date   2008-11-11
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/timed_semaphore.h>

#include <sys/time.h>

namespace Libc {
extern time_t read_rtc();
}

extern "C" __attribute__((weak))
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	static bool read_rtc = false;
	static time_t rtc = 0;

	if (!read_rtc) {
		rtc = Libc::read_rtc();
		read_rtc = true;
	}

	Genode::Alarm::Time time = Genode::Timeout_thread::alarm_timer()->time();

	if (tv) {
		tv->tv_sec = rtc + time / 1000;
		tv->tv_usec = (time % 1000) * 1000;
	}

	return 0;
}
