/*
 * \brief  Timer accuracy test for Linux
 * \author Norman Feske
 * \date   2010-03-09
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <base/printf.h>

/* Linux includes */
#include <linux_syscalls.h>
#include <sys/time.h>

inline int lx_gettimeofday(struct timeval *tv, struct timeval *tz)
{
	return lx_syscall(SYS_gettimeofday, tv, tz);
}


using namespace Genode;

int main(int, char **)
{
	printf("--- timer accuracy test ---\n");

	static Timer::Connection timer;

	static struct timeval old_tv;
	static struct timeval new_tv;

	enum { ROUNDS = 10 };

	for (unsigned i = 0; i < ROUNDS; ++i) {
		printf("Round [%d/%d] - calling msleep for 5 seconds...\n", i + 1, ROUNDS);

		lx_gettimeofday(&old_tv, 0);

		timer.msleep(5*1000);

		lx_gettimeofday(&new_tv, 0);

		printf("old: %ld seconds %ld microseconds\n", old_tv.tv_sec, old_tv.tv_usec);
		printf("new: %ld seconds %ld microseconds\n", new_tv.tv_sec, new_tv.tv_usec);
		printf("diff is about %ld seconds\n", new_tv.tv_sec - old_tv.tv_sec);
	}

	printf("--- finished timer accuracy test ---\n");
	return 0;
}
