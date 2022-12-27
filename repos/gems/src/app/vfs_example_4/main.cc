/*
 * \brief  Read VFS <inline> files from POSIX/libc
 * \author Martin Stein
 * \date   2021-06-21
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int, char **)
{
	static char buf[128];
	int fd = open("/friendly/greetings", O_RDWR);
	if (fd < 0) {
		printf("Error: could not open file\n");
		return -1;
	}
	int count = read(fd, buf, sizeof(buf) - 1);
	printf("Read %d bytes: %s\n", count, buf);

	int err = write(fd, buf, sizeof(buf) - 1);
	printf("Write returned %d\n", err);
	return 0;
}
