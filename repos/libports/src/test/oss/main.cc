/*
 * \brief  OSS test
 * \author Christian Prochaska
 * \date   2021-10-07
 */

/*
 * Copyright (C) 2021-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/soundcard.h>

int main(int argc, char **argv)
{
	static char buf[2048];

	int fd = open("/dev/dsp", O_RDWR);

	if (fd < 0) {
		printf("Error: could not open /dev/dsp\n");
		return -1;
	}

	for (;;) {

		ssize_t bytes_read = read(fd, buf, sizeof(buf));

		if (bytes_read != sizeof(buf)) {
			printf("Error: read error\n");
			return -1;
		}

		ssize_t bytes_written = write(fd, buf, sizeof(buf));

		if (bytes_written != sizeof(buf)) {
			printf("Error: write error\n");
			return -1;
		}
	}

	return 0;
}
