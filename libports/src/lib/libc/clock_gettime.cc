/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \date   2010-05-19
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <os/timed_semaphore.h>

#include <sys/time.h>

extern "C" __attribute__((weak))
int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	Genode::Alarm::Time time = Genode::Timeout_thread::alarm_timer()->time();

	if (tp) {
		tp->tv_sec = time / 1000;
		tp->tv_nsec = (time % 1000) * (1000 * 1000);
	}

	return 0;
}
