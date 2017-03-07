/*
 * \brief  libc_block test
 * \author Josef Soentgen
 * \date   2013-11-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


static char buf[16384];
static char str[] = "deadbeef";

int main(int argc, char *argv[])
{
	int fd;
	ssize_t n;
	off_t offset;

	printf("--- start test ---\n");

	fd = open("/dev/blkdev", O_RDWR);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	offset = lseek(fd, 8193, SEEK_SET);
	printf("offset: %lld\n", (long long)offset);

	n = write(fd, str, sizeof (str));

	offset = lseek(fd, 8193, SEEK_SET);
	printf("offset: %lld\n", (long long)offset);
	n = read(fd, buf, sizeof (str));
	printf("bytes: %zd\n", n);
	for (size_t i = 0; i < sizeof (str); i++)
		printf("%c ", buf[i]);
	printf("\n");

	offset = lseek(fd, 16384, SEEK_SET);
	n = write(fd, buf, sizeof (buf));
	if (n != sizeof (buf))
		printf("error write mismatch: %zd != %zu\n", n, sizeof (buf));

	offset = lseek(fd, 4060, SEEK_SET);
	n = write(fd, buf, sizeof (buf) / 2);
	if (n != sizeof (buf)/2)
		printf("error write mismatch: %zd != %zu\n", n, sizeof (buf)/2);

	offset = lseek(fd, 2342, SEEK_SET);
	n = read(fd, buf, sizeof (buf));
	if (n != sizeof (buf))
		printf("error read mismatch: %zd != %zu\n", n, sizeof (buf));

	close(fd);

	printf("--- test finished ---\n");

	return 0;
}
