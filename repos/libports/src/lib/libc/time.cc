/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2010-05-19
 */

/*
 * Copyright (C) 2010-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <vfs/vfs_handle.h>

/* Genode-internal includes */
#include <base/internal/unmanaged_singleton.h>

/* libc includes */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* libc-internal includes */
#include <internal/errno.h>
#include <internal/init.h>
#include <internal/current_time.h>
#include <internal/watch.h>

static Libc::Current_time      *_current_time_ptr;
static Libc::Current_real_time *_current_real_time_ptr;


void Libc::init_time(Current_time      &current_time,
                     Current_real_time &current_real_time)
{
	_current_time_ptr      = &current_time;
	_current_real_time_ptr = &current_real_time;
}


using namespace Libc;


extern "C" __attribute__((weak))
int clock_gettime(clockid_t clk_id, struct timespec *ts)
{
	typedef Libc::uint64_t uint64_t;

	if (!ts) return Errno(EFAULT);

	struct Missing_call_of_init_time : Exception { };

	auto current_time = [&] ()
	{
		if (!_current_time_ptr)
			throw Missing_call_of_init_time();

		return _current_time_ptr->current_time();
	};


	/* initialize timespec just in case users do not check for errors */
	ts->tv_sec  = 0;
	ts->tv_nsec = 0;

	switch (clk_id) {

	/* IRL wall-time */
	case CLOCK_REALTIME:
	case CLOCK_SECOND: /* FreeBSD specific */
	{
		if (!_current_real_time_ptr)
			throw Missing_call_of_init_time();

		if (!_current_real_time_ptr->has_real_time()) {
			warning("clock_gettime(): missing real-time clock");
			return Errno(EINVAL);
		}

		*ts = _current_real_time_ptr->current_real_time();
		break;
	}

	/* component uptime */
	case CLOCK_MONOTONIC:
	case CLOCK_UPTIME:
	{
		uint64_t us = current_time().trunc_to_plain_us().value;

		ts->tv_sec  = us / (1000*1000);
		ts->tv_nsec = (us % (1000*1000)) * 1000;
		break;
	}

	default:
		return Errno(EINVAL);
	}

	return 0;
}


extern "C" __attribute__((weak, alias("clock_gettime")))
int __sys_clock_gettime(clockid_t clk_id, struct timespec *ts);


extern "C" __attribute__((weak))
int gettimeofday(struct timeval *tv, struct timezone *)
{
	if (!tv) return 0;

	struct timespec ts;

	if (int ret = clock_gettime(CLOCK_REALTIME, &ts))
		return ret;

	tv->tv_sec  = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	return 0;
}


extern "C" __attribute__((weak, alias("gettimeofday")))
int __sys_gettimeofday(struct timeval *tv, struct timezone *);


extern "C"
clock_t clock()
{
	error(__func__, " not implemented, use 'clock_gettime' instead");
	return -1;
}
