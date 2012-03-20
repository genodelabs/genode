/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \date   2012-03-20
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/timed_semaphore.h>

#include <sys/time.h>

using namespace Genode;

extern "C" __attribute__((weak))
int _nanosleep(const struct timespec *req, struct timespec *rem)
{
	Genode::Alarm::Time sleep_msec = (req->tv_sec * 1000) + (req->tv_nsec / 1000000);

	/* Timed_semaphore does not support timeouts < 10ms */
	if (sleep_msec < 10)
		sleep_msec = 10;

	Timed_semaphore sem(0);
	try {
		sem.down(sleep_msec);
	} catch(Timeout_exception) {
	}

	if (rem) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}

	return 0;
}
