/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2010-05-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Libc includes */
#include <sys/time.h>

#include "task.h"
#include "libc_errno.h"

namespace Libc { time_t read_rtc(); }


extern "C" __attribute__((weak))
int clock_gettime(clockid_t clk_id, struct timespec *ts)
{
	if (!ts) return 0;

	static bool   initial_rtc_requested = false;
	static time_t initial_rtc = 0;
	static unsigned long t0 = 0;

	ts->tv_sec  = 0;
	ts->tv_nsec = 0;

	/* try to read rtc once */
	if (!initial_rtc_requested) {
		initial_rtc_requested = true;

		initial_rtc = Libc::read_rtc();

		if (initial_rtc)
			t0 = Libc::current_time();
	}

	if (!initial_rtc)
		return Libc::Errno(EINVAL);

	unsigned long time = Libc::current_time() - t0;

	ts->tv_sec  = initial_rtc + time/1000;
	ts->tv_nsec = (time % 1000) * (1000*1000);

	return 0;
}


extern "C" __attribute__((weak))
int gettimeofday(struct timeval *tv, struct timezone *)
{
	if (!tv) return 0;

	struct timespec ts;

	if (int ret = clock_gettime(0, &ts))
		return ret;

	tv->tv_sec  = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	return 0;
}
