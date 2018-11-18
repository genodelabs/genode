/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \date   2012-03-20
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Libc includes */
#include <sys/time.h>

#include "task.h"

extern "C" __attribute__((weak))
int _nanosleep(const struct timespec *req, struct timespec *rem)
{
	using namespace Libc;

	Microseconds sleep_us(req->tv_sec*1000*1000 + req->tv_nsec/1000);

	if (!sleep_us.value) return 0;

	struct Check : Libc::Suspend_functor { bool suspend() override { return true; } } check;
	do { sleep_us = Libc::suspend(check, sleep_us); } while (sleep_us.value);

	if (rem) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}

	return 0;
}


extern "C" __attribute__((weak))
int nanosleep(const struct timespec *req, struct timespec *rem)
{
	return _nanosleep(req, rem);
}
