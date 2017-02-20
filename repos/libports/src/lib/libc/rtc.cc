/*
 * \brief  C-library back end
 * \author Josef Soentgen
 * \date   2014-08-20
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/printf.h>
#include <util/string.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>


namespace Libc {
	extern char const *config_rtc();
	time_t read_rtc();
}

time_t Libc::read_rtc()
{
	time_t rtc = 0;

	if (!Genode::strcmp(Libc::config_rtc(), "")) {
		PWRN("%s: rtc not configured, returning %lld", __func__, (long long)rtc);
		return rtc;
	}

	int fd = open(Libc::config_rtc(), O_RDONLY);
	if (fd == -1) {
		PWRN("%s: %s not readable, returning %lld", __func__, Libc::config_rtc(), (long long)rtc);
		return rtc;
	}

	char buf[32];
	ssize_t n = read(fd, buf, sizeof(buf));
	if (n > 0) {
		buf[n - 1] = '\0';
		struct tm tm;
		Genode::memset(&tm, 0, sizeof(tm));

		if (strptime(buf, "%Y-%m-%d %R", &tm)) {
			rtc = mktime(&tm);
			if (rtc == (time_t)-1)
				rtc = 0;
		}
	}

	close(fd);

	return rtc;
}
