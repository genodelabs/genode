/*
 * \brief  Writing files in a VFS <ram> FS
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
	static char const *str_1 = "A text to be remembered.";
	static char const *str_2 = "that will soon be forgotten.";
	static char buf[128];

	/* create a new file inside the RAM file system */
	int fd = open("/my_notes/entry_1", O_RDWR | O_CREAT);
	if (fd < 0) {
		printf("Error: could not open file\n");
		return -1;
	}
	/* write string to file */
	int ret = write(fd, str_1, strlen(str_1));
	printf("Wrote %d bytes at offset 0\n", ret);

	/* seek to the beginning of the file and print the whole content */
	lseek(fd, 0, SEEK_SET);
	ret = read(fd, buf, sizeof(buf) - 1);
	printf("Read %d bytes: %s\n", ret, buf);

	/* overwrite file partially */
	lseek(fd, 7, SEEK_SET);
	ret = write(fd, str_2, strlen(str_2));
	printf("Wrote %d bytes at offset 7\n", ret);

	/* seek to the beginning of the file and print the whole content */
	lseek(fd, 0, SEEK_SET);
	ret = read(fd, buf, sizeof(buf) - 1);
	printf("Read %d bytes: %s\n", ret, buf);

	return 0;
}
