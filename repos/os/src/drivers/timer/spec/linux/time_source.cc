/*
 * \brief  Time source that uses sleeping by the means of the kernel
 * \author Norman Feske
 * \author Martin Stein
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <linux_syscalls.h>
#include <sys/time.h>

/* local includes */
#include <time_source.h>

using namespace Genode;
using Microseconds = Genode::Time_source::Microseconds;


inline int lx_gettimeofday(struct timeval *tv, struct timeval *tz) {
	return lx_syscall(SYS_gettimeofday, tv, tz); }


Microseconds Timer::Time_source::max_timeout() const
{
	Lock::Guard lock_guard(_lock);
	return Microseconds(1000 * 1000);
}


Microseconds Timer::Time_source::curr_time() const
{
	struct timeval tv;
	lx_gettimeofday(&tv, 0);
	return Microseconds(tv.tv_sec * 1000 * 1000 + tv.tv_usec);
}


void Timer::Time_source::_usleep(unsigned long us)
{
	struct timespec ts;
	ts.tv_sec  =  us / (1000 * 1000);
	ts.tv_nsec = (us % (1000 * 1000)) * 1000;

	if (lx_nanosleep(&ts, &ts) != 0)
		throw Blocking_canceled();
}
