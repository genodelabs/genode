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

extern "C" __attribute__((weak))
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	Genode::Alarm::Time time = Genode::Timeout_thread::alarm_timer()->time();

	if (tv) {
		tv->tv_sec = time / 1000;
		tv->tv_usec = (time % 1000) * 1000;
	}

	return 0;
}
