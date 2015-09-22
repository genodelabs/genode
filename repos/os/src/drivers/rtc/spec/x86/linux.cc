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
#include <time.h>

#include "rtc.h"


Rtc::Timestamp Rtc::get_time(void)
{
	Timestamp ts { 0 };

	time_t t       = time(NULL);
	struct tm *utc = gmtime(&t);
	if (utc) {
		ts.second = utc->tm_sec;
		ts.minute = utc->tm_min;
		ts.hour   = utc->tm_hour;
		ts.day    = utc->tm_mday;
		ts.month  = utc->tm_mon + 1;
		ts.year   = utc->tm_year + 1900;
	}

	return ts;
}
