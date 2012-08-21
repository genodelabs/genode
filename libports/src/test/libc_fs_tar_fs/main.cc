/*
 * \brief  libc plugin read-only test
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2011-08-20
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>

/* libc includes */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#define CALL_AND_CHECK(ret, operation, condition, info_string, ...) \
	printf("calling " #operation " " info_string "\n", ##__VA_ARGS__); \
	ret = operation; \
	if (condition) { \
		printf(#operation " succeeded\n"); \
	} else { \
		printf(#operation " failed, " #ret "=%ld, errno=%d\n", (long)ret, errno); \
		return -1; \
	}


int main(int argc, char *argv[])
{
	int ret, fd;
	ssize_t count;

	char const *dir_name      = "/testdir";
	char const *file_name     = "test.tst";
	char const *pattern       = "a single line of text";

	size_t      pattern_size  = strlen(pattern);

	unsigned int iterations = 1;

	try {
		Genode::config()->xml_node().sub_node("iterations").attribute("value").value(&iterations);
	} catch(...) { }

	for (unsigned int i = 0; i < iterations; i++) {

		/* change to new directory */
		CALL_AND_CHECK(ret, chdir(dir_name), ret == 0, "dir_name=%s", dir_name);

		/* query file status of file */
		struct stat stat_buf;
		CALL_AND_CHECK(ret, stat(file_name, &stat_buf), ret == 0, "file_name=%s", file_name);
		printf("file size: %u bytes\n", (unsigned)stat_buf.st_size);
		struct tm *file_time = gmtime(&stat_buf.st_mtime);
		printf("last modified: %04u-%02u-%02u %02u:%02u:%02u\n",
			   file_time->tm_year, file_time->tm_mon, file_time->tm_mday,
			   file_time->tm_hour, file_time->tm_min, file_time->tm_sec);

		/* read and verify file content */
		CALL_AND_CHECK(fd, open(file_name, O_RDONLY), fd >= 0, "file_name=%s", file_name);
		static char buf[512];
		CALL_AND_CHECK(count, read(fd, buf, sizeof(buf)), (size_t)count == pattern_size, "");
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");
		printf("content of file: \"%s\"\n", buf);
		if (strcmp(buf, pattern) != 0) {
			printf("unexpected content of file\n");
			return -1;
		} else {
			printf("file content is correct\n");
		}

		/* read directory entries */
		DIR *dir;
		CALL_AND_CHECK(dir, opendir(dir_name), dir, "dir_name=\"%s\"", dir_name);
		printf("calling readdir()\n");
		for (;;) {
			struct dirent *dirent = readdir(dir);
			if (dirent) {
				if (dirent->d_type == DT_DIR)
					printf("found directory %s\n", dirent->d_name);
				else
					printf("found file %s\n", dirent->d_name);
			} else {
				printf("no (more) direntries found\n");
				break;
			}
		}

		if (i < (iterations - 1))
			sleep(2);
	}

	printf("test finished\n");

	return 0;
}
