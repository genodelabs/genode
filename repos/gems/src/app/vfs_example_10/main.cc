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
	{
		char const *path { "/my_rw_state" };
		char const *str { "Much better now." };
		char buf[128];

		/* open copy of file in RAM file system */
		int fd = open(path, O_RDWR | O_CREAT);
		if (fd < 0) {
			printf("Error: could not open file\n");
			return -1;
		}
		/* print the whole file content */
		int ret = read(fd, buf, sizeof(buf) - 1);
		buf[ret] = 0;
		printf("Read_bytes 0..%d of %s: \"%s\"\n", ret - 1, path, buf);

		/* write string to file */
		lseek(fd, 0, SEEK_SET);
		ret = write(fd, str, strlen(str));
		printf("Wrote bytes 0..%d of %s\n", ret - 1, path);

		/* print the whole file content again */
		lseek(fd, 0, SEEK_SET);
		ret = read(fd, buf, sizeof(buf) - 1);
		buf[ret] = 0;
		printf("Read_bytes 0..%d of %s: \"%s\"\n", ret - 1, path, buf);
	}

	{
		char const *path { "/my_dir/new_file" };
		char const *str { "A new file in a FAT FS." };
		char buf[128];

		/* create new file in a FAT file system */
		int fd = open(path, O_RDWR | O_CREAT);
		if (fd < 0) {
			printf("Error: could not open file\n");
			return -1;
		}
		/* write string to file */
		int ret = write(fd, str, strlen(str));
		printf("Wrote bytes 0..%d of %s\n", ret - 1, path);

		/* print the whole file content */
		lseek(fd, 0, SEEK_SET);
		ret = read(fd, buf, sizeof(buf) - 1);
		buf[ret] = 0;
		printf("Read_bytes 0..%d of %s: \"%s\"\n", ret - 1, path, buf);
	}

	{
		/* open file in ROM file system */
		char const *path { "/my_dir/my_ro_state" };
		char const *str = "Nice try.";
		char buf[128];
		int fd = open(path, O_RDWR);

		/* print the whole file content */
		int ret = read(fd, buf, sizeof(buf) - 1);
		buf[ret] = 0;
		printf("Read_bytes 0..%d of %s: \"%s\"\n", ret - 1, path, buf);

		/* try to write string to file */
		lseek(fd, 0, SEEK_SET);
		ret = write(fd, str, strlen(str));
		printf("Wrote bytes 0..%d of %s\n", ret - 1, path);

		/* print the whole file content again */
		lseek(fd, 0, SEEK_SET);
		ret = read(fd, buf, sizeof(buf) - 1);
		buf[ret] = 0;
		printf("Read_bytes 0..%d of %s: \"%s\"\n", ret - 1, path, buf);
	}
	return 0;
}
