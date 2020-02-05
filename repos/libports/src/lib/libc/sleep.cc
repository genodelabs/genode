/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \author Emery Hemingway
 * \date   2012-03-20
 */

/*
 * Copyright (C) 2008-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <sys/time.h>

/* libc-internal includes */
#include <internal/suspend.h>
#include <internal/init.h>

using namespace Libc;


static Suspend *_suspend_ptr;


void Libc::init_sleep(Suspend &suspend)
{
	_suspend_ptr = &suspend;
}


static void millisleep(Genode::uint64_t timeout_ms)
{
	Genode::uint64_t remaining_ms = timeout_ms;

	struct Missing_call_of_init_sleep : Exception { };
	if (!_suspend_ptr)
		throw Missing_call_of_init_sleep();

	struct Check : Libc::Suspend_functor
	{
		bool suspend() override { return true; }

	} check;

	while (remaining_ms > 0)
		remaining_ms = _suspend_ptr->suspend(check, remaining_ms);
}


extern "C" __attribute__((weak))
int nanosleep(const struct timespec *req, struct timespec *rem)
{
	/* XXX nanosleep({0,0}) may yield but is not required to do so */
	if (!req->tv_sec && !req->tv_nsec) return 0;

	Genode::uint64_t sleep_ms = (Genode::uint64_t)req->tv_sec*1000;
	if (req->tv_nsec)
		sleep_ms += req->tv_nsec / 1000000;

	/* sleep at least 1 ms */
	millisleep(sleep_ms ? : 1);

	if (rem)
		*rem = { 0, 0 };

	return 0;
}

extern "C" __attribute__((alias("nanosleep")))
int __sys_nanosleep(const struct timespec *req, struct timespec *rem);


extern "C" __attribute__((weak))
int clock_nanosleep(clockid_t clock_id, int flags,
                    const struct timespec *rqt,
                    struct timespec *rmt)
{
	if (flags & TIMER_ABSTIME) {
		struct timespec now_ts { 0 };
		if (clock_gettime(clock_id, &now_ts) != 0) {
			error(__func__, ": RTC device not configured");
			return -1;
		}

		if (now_ts.tv_sec <= rqt->tv_sec && now_ts.tv_nsec < rqt->tv_nsec) {
			struct timespec new_ts = {
				.tv_sec  = rqt->tv_sec  - now_ts.tv_sec,
				.tv_nsec = rqt->tv_nsec - now_ts.tv_nsec,
			};
			return nanosleep(&new_ts, rmt);
		}
		return 0;
	}
	return nanosleep(rqt, rmt);
}

extern "C" __attribute__((alias("clock_nanosleep")))
int __sys_clock_nanosleep(clockid_t clock_id, int flags,
                          const struct timespec *rqt,
                          struct timespec *rmt);


extern "C" __attribute__((weak))
unsigned int sleep(unsigned int seconds)
{
	/* XXX sleep(0) may yield but is not required to do so */
	if (!seconds) return 0;

	Libc::uint64_t const sleep_ms = (Libc::uint64_t)seconds*1000;

	/* sleep at least 1 ms */
	millisleep(sleep_ms ? : 1);
	return 0;
}


extern "C" __attribute__((weak))
int usleep(useconds_t useconds)
{
	/* XXX usleep(0) may yield but is not required to do so */
	if (!useconds) return 0;

	Libc::uint64_t const sleep_ms = (Libc::uint64_t)useconds/1000;

	/* sleep at least 1 ms */
	millisleep(sleep_ms ? : 1);
	return 0;
}

extern "C" __attribute__((alias("usleep")))
int _usleep(useconds_t useconds);
