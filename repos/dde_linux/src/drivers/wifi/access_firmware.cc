/*
 * \brief  Firmware I/O functions
 * \author Josef Soentgen
 * \date   2023-05-05
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <libc/component.h>

/* libc includes */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* local includes */
#include "access_firmware.h"


Stat_firmware_result access_firmware(char const *path)
{
	Stat_firmware_result result { false, 0 };


	Libc::with_libc([&] {
		struct stat stat_buf { };
		int const err = ::stat(path, &stat_buf);
		if (!err) {
			result.success = true;
			result.length  = stat_buf.st_size;
		}
	});

	return result;
}


Read_firmware_result read_firmware(char const *path, char *dst, size_t dst_len)
{
	Read_firmware_result result { false };

	Libc::with_libc([&] {
		int const fd = ::open(path, O_RDONLY);
		if (fd < 0)
			return;

		size_t total  = 0;
		size_t remain = dst_len;
		do {
			ssize_t const cur = ::read(fd, dst, remain);
			if (cur < ssize_t(0) && errno != EINTR)
				break;
			if (cur == ssize_t(0))
				break;
			total += cur;
			dst   += cur;
			remain -= (size_t)cur;

		} while (total < dst_len);

		(void)close(fd);

		if (total > 0 && (size_t)total != dst_len)
			return;

		result.success = true;
	});

	return result;
}
