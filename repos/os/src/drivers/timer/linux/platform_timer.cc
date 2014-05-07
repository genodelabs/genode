/*
 * \brief  Linux-specific sleep implementation
 * \author Norman Feske
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux syscall bindings */
#include <linux_syscalls.h>

/* local includes */
#include "timer_session_component.h"

/* Linux includes */
#include <linux_syscalls.h>
#include <sys/time.h>

inline int lx_gettimeofday(struct timeval *tv, struct timeval *tz)
{
	return lx_syscall(SYS_gettimeofday, tv, tz);
}


unsigned long Platform_timer::max_timeout()
{
	Genode::Lock::Guard lock_guard(_lock);
	return 1000*1000;
}


unsigned long Platform_timer::curr_time() const
{
	struct timeval tv;
	lx_gettimeofday(&tv, 0);
	return tv.tv_sec*1000*1000 + tv.tv_usec;
}


void Platform_timer::_usleep(unsigned long usecs)
{
	struct timespec ts;
	ts.tv_sec  =  usecs / (1000*1000);
	ts.tv_nsec = (usecs % (1000*1000)) * 1000;

	if (lx_nanosleep(&ts, &ts) != 0)
		throw Genode::Blocking_canceled();
}
