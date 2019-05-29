/*
 * \brief  Component providing a Terminal session via SSH
 * \author Josef Soentgen
 * \author Pirmin Duss
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "util.h"

char const *Util::get_time()
{
	static char buffer[32];

	char const *p = "<invalid date>";
	Libc::with_libc([&] {
		struct timespec ts;
		if (clock_gettime(0, &ts)) { return; }

		struct tm *tm = localtime((time_t*)&ts.tv_sec);
		if (!tm) { return; }

		size_t const n = strftime(buffer, sizeof(buffer), "%F %H:%M:%S", tm);
		if (n > 0 && n < sizeof(buffer)) { p = buffer; }
	}); /* Libc::with_libc */

	return p;
}
