/*
 * \brief  Linux RTC pseudo driver
 * \author Christian Helmuth
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <sys/time.h>

#include "rtc.h"


Genode::uint64_t Rtc::get_time(void)
{
	struct timeval now { };

	gettimeofday(&now, nullptr);

	return now.tv_sec * 1000000ULL + now.tv_usec;
}
