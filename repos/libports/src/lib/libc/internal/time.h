/*
 * \brief  Libc-internal time utilities
 * \author Christian Helmuth
 * \date   2020-01-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__TIME_H_
#define _LIBC__INTERNAL__TIME_H_

/* libc includes */
#include <time.h>

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {
	inline uint64_t calculate_relative_timeout_ms(timespec abs_now, timespec abs_timeout);
}

/**
 * Calculate relative timeout in milliseconds from 'abs_now' to 'abs_timeout'
 *
 * Returns 0 if timeout already expired.
 */
Libc::uint64_t Libc::calculate_relative_timeout_ms(timespec abs_now, timespec abs_timeout)
{
	enum { S_IN_MS = 1000ULL, S_IN_NS = 1000 * 1000 * 1000ULL };

	if (abs_now.tv_nsec >= S_IN_NS) {
		abs_now.tv_sec  += abs_now.tv_nsec / S_IN_NS;
		abs_now.tv_nsec  = abs_now.tv_nsec % S_IN_NS;
	}
	if (abs_timeout.tv_nsec >= S_IN_NS) {
		abs_timeout.tv_sec  += abs_timeout.tv_nsec / S_IN_NS;
		abs_timeout.tv_nsec  = abs_timeout.tv_nsec % S_IN_NS;
	}

	/* check whether absolute timeout is in the past */
	if (abs_now.tv_sec > abs_timeout.tv_sec)
		return 0;

	uint64_t diff_ms = (abs_timeout.tv_sec - abs_now.tv_sec) * S_IN_MS;
	uint64_t diff_ns = 0;

	if (abs_timeout.tv_nsec >= abs_now.tv_nsec)
		diff_ns = abs_timeout.tv_nsec - abs_now.tv_nsec;
	else {
		/* check whether absolute timeout is in the past */
		if (diff_ms == 0)
			return 0;
		diff_ns  = S_IN_NS - abs_now.tv_nsec + abs_timeout.tv_nsec;
		diff_ms -= S_IN_MS;
	}

	diff_ms += diff_ns / 1000 / 1000;

	/* if there is any diff then let the timeout be at least 1 MS */
	if (diff_ms == 0 && diff_ns != 0)
		return 1;

	return diff_ms;
}


#endif /* _LIBC__INTERNAL__TIME_H_ */
