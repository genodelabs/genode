/*
 * \brief  Accessing VFS files imported via <import> into a <ram> FS
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
#include <string.h>

int main(int, char **)
{
	static char const *str = "ful woman.";
	static char buf[128];

	/* open the file and print the whole content */
	int fd = open("/x/y/z", O_RDWR);
	if (fd < 0) {
		printf("Error: could not open file\n");
		return -1;
	}
	int ret = read(fd, buf, sizeof(buf) - 1);
	printf("Read %d bytes: %s\n", ret, buf);

	/* partially overwrite the file */
	lseek(fd, 21, SEEK_SET);
	ret = write(fd, str, strlen(str));
	printf("Wrote %d bytes at offset 0\n", ret);

	/* seek to the beginning of the file and print the whole content again */
	lseek(fd, 0, SEEK_SET);
	ret = read(fd, buf, sizeof(buf) - 1);
	printf("Read %d bytes: %s\n", ret, buf);

	return 0;
}
