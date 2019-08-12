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

/* Libc includes */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



#include "task.h"
#include "libc_errno.h"

/* Genode includes */
#include <base/log.h>
#include <vfs/vfs_handle.h>


namespace Libc {
	extern char const *config_rtc();
}


struct Rtc : Vfs::Watch_response_handler
{
	Vfs::Vfs_watch_handle *_watch_handle { nullptr };
	char const            *_file         { nullptr };
	bool                  _read_file     { true };
	time_t                _rtc_value     { 0 };

	Rtc(char const *rtc_file)
	: _file(rtc_file)
	{
		if (!Genode::strcmp(_file, "")) {
			Genode::warning("rtc not configured, returning ", _rtc_value);
			return;
		}

		_watch_handle = Libc::watch(_file);
		if (_watch_handle) {
			_watch_handle->handler(this);
		}
	}

	/******************************************
	 ** Vfs::Watch_reponse_handler interface **
	 ******************************************/

	void watch_response() override
	{
		_read_file = true;
	}

	time_t read()
	{
		if (!_file) { return 0; }

		/* return old value */
		if (!_read_file) { return _rtc_value; }

		_read_file = false;

		int fd = open(_file, O_RDONLY);
		if (fd == -1) {
			Genode::warning(Genode::Cstring(Libc::config_rtc()), " not readable, returning ", _rtc_value);
			return _rtc_value;
		}

		char buf[32];
		ssize_t n = ::read(fd, buf, sizeof(buf));
		if (n > 0) {
			buf[n - 1] = '\0';
			struct tm tm;
			Genode::memset(&tm, 0, sizeof(tm));

			if (strptime(buf, "%Y-%m-%d %R", &tm)) {
				_rtc_value = mktime(&tm);
				if (_rtc_value == (time_t)-1)
					_rtc_value = 0;
			}
		}

		close(fd);

		uint64_t const ts_value =
			Libc::current_time().trunc_to_plain_ms().value;
		_rtc_value += (time_t)ts_value / 1000;

		return _rtc_value;
	}
};


extern "C" __attribute__((weak))
int clock_gettime(clockid_t clk_id, struct timespec *ts)
{
	if (!ts) return Libc::Errno(EFAULT);

	/* initialize timespec just in case users do not check for errors */
	ts->tv_sec  = 0;
	ts->tv_nsec = 0;

	switch (clk_id) {

	/* IRL wall-time */
	case CLOCK_REALTIME:
	case CLOCK_SECOND: /* FreeBSD specific */
	{
		static Rtc rtc(Libc::config_rtc());

		time_t const rtc_value = rtc.read();
		if (!rtc_value) return Libc::Errno(EINVAL);

		Genode::uint64_t const time = Libc::current_time().trunc_to_plain_ms().value;

		ts->tv_sec  = rtc_value + time/1000;
		ts->tv_nsec = (time % 1000) * (1000*1000);
		break;
	}

	/* component uptime */
	case CLOCK_MONOTONIC:
	case CLOCK_UPTIME:
	{
		Genode::uint64_t us = Libc::current_time().trunc_to_plain_us().value;

		ts->tv_sec  = us / (1000*1000);
		ts->tv_nsec = (us % (1000*1000)) * 1000;
		break;
	}

	default:
		return Libc::Errno(EINVAL);
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
	Genode::error(__func__, " not implemented, use 'clock_gettime' instead");
	return -1;
}
