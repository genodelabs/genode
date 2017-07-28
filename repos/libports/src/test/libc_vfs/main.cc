/*
 * \brief  Libc file-system test
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2011-05-27
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <libc/component.h>

/* libc includes */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


struct Test_failed : Genode::Exception { };

#define CALL_AND_CHECK(ret, operation, condition, info_string, ...) \
	printf("calling " #operation " " info_string "\n", ##__VA_ARGS__); \
	ret = operation; \
	if (condition) { \
		printf(#operation " succeeded\n"); \
	} else { \
		printf(#operation " failed, " #ret "=%ld, errno=%d\n", (long)ret, errno); \
		throw Test_failed(); \
	}


static void test_write_read(Genode::Xml_node node)
{
	size_t rounds      = 4;
	size_t size        = 4*1024*1024;
	size_t buffer_size = 32*1024;

	try {
		Genode::Xml_node config = node.sub_node("write-read");

		try { config.attribute("rounds").value(&rounds); } catch (...) { }

		Genode::Number_of_bytes n;
		try {
			config.attribute("size").value(&n);
			size = n;
		} catch (...) { }
		try {
			config.attribute("buffer_size").value(&n);
			buffer_size = n;
		} catch (...) { }
	} catch (...) { }

	void *buf = malloc(buffer_size);
	char const *file_name = "write_read.tst";

	printf("write-read test: %zu rounds of %zu MiB (buffer size %zu)\n",
	       rounds, size/(1024*1024), buffer_size);

	for (unsigned round = 0; buf && round < rounds; ++round) {
		printf("starting round %u\n", round);

		unlink(file_name);

		int fd = open(file_name, O_CREAT | O_RDWR);

		memset(buf, round, buffer_size);

		/* write-read i_max times the buffer */
		unsigned const i_max = size/buffer_size;
		for (unsigned i = 0; i < i_max; ++i) write(fd, buf, buffer_size);
		lseek(fd, SEEK_SET, 0);
		for (unsigned i = 0; i < i_max; ++i) read(fd, buf, buffer_size);

		close(fd);
		printf("finished round %u\n", round);
	}

	free(buf);
}


static void test(Genode::Xml_node node)
{
	int ret, fd;
	ssize_t count;

	char const *dir_name      = "testdir";
	char const *dir_name2     = "testdir2";
	char const *file_name     = "test.tst";
	char const *file_name2    = "test2.tst";
	char const *file_name3    = "test3.tst";
	char const *file_name4    = "test4.tst";
	char const *file_name5    = "test5.tst";
	char const *pattern       = "a single line of text";

	size_t      pattern_size  = strlen(pattern) + 1;

	unsigned int iterations = 1;

	try {
		node.sub_node("iterations").attribute("value").value(&iterations);
	} catch(...) { }

	for (unsigned int i = 0; i < iterations; i++) {

		/* create directory (short name) */
		CALL_AND_CHECK(ret, mkdir(dir_name, 0777), ((ret == 0) || (errno == EEXIST)), "dir_name=%s", dir_name);

		/* try to create again */
		CALL_AND_CHECK(ret, mkdir(dir_name, 0777), (errno == EEXIST), "dir_name=%s", dir_name);

		/* change to new directory */
		CALL_AND_CHECK(ret, chdir(dir_name), ret == 0, "dir_name=%s", dir_name);

		/* create subdirectory with relative path */
		CALL_AND_CHECK(ret, mkdir(dir_name2, 0777), ((ret == 0) || (errno == EEXIST)), "dir_name=%s", dir_name2);

		/* write pattern to a file */
		CALL_AND_CHECK(fd, open(file_name, O_CREAT | O_WRONLY), fd >= 0, "file_name=%s", file_name);
		CALL_AND_CHECK(count, write(fd, pattern, pattern_size), (size_t)count == pattern_size, "");
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");

		/* open the file with O_CREAT again (should have no effect on the file) */
		CALL_AND_CHECK(fd, open(file_name, O_CREAT | O_WRONLY), fd >= 0, "file_name=%s", file_name);
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");

		/* query file status of new file */
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
			throw Test_failed();
		} else {
			printf("file content is correct\n");
		}

		/* move file (target does not exist) */
		CALL_AND_CHECK(fd, open(file_name5, O_CREAT | O_WRONLY), fd >= 0, "file_name=%s", file_name5);
		CALL_AND_CHECK(ret, rename(file_name5, "x"), ret == 0, "file_name=%s", file_name5);
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");

		/* move file (target already exists) */
		CALL_AND_CHECK(fd, open(file_name5, O_CREAT | O_WRONLY), fd >= 0, "file_name=%s", file_name5);
		CALL_AND_CHECK(ret, rename(file_name5, "x"), ret == 0, "file_name=%s", file_name5);
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");

		/* test 'pread()' and 'pwrite()' */
		CALL_AND_CHECK(fd, open(file_name2, O_CREAT | O_WRONLY), fd >= 0, "file_name=%s", file_name2);
		/* write "a single line of" */
		CALL_AND_CHECK(count, pwrite(fd, pattern, (pattern_size - 6), 0), (size_t)count == (pattern_size - 6), "");
		/* write "line of text" at offset 9 */
		CALL_AND_CHECK(count, pwrite(fd, &pattern[9], (pattern_size - 9), 9), (size_t)count == (pattern_size - 9), "");
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");
		CALL_AND_CHECK(fd, open(file_name2, O_RDONLY), fd >= 0, "file_name=%s", file_name2);
		memset(buf, 0, sizeof(buf));
		/* read "single line of text" from offset 2 */
		CALL_AND_CHECK(count, pread(fd, buf, sizeof(buf), 2), (size_t)count == (pattern_size - 2), "");
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");
		printf("content of file: \"%s\"\n", buf);
		if (strcmp(buf, &pattern[2]) != 0) {
			printf("unexpected content of file\n");
			throw Test_failed();
		} else {
			printf("file content is correct\n");
		}

		/* test 'readv()' and 'writev()' */
		CALL_AND_CHECK(fd, open(file_name3, O_CREAT | O_WRONLY), fd >= 0, "file_name=%s", file_name3);
		struct iovec iov[2];
		/* write "a single line" */
		iov[0].iov_base = (void*)pattern;
		iov[0].iov_len = 13;
		/* write " line of text" */
		iov[1].iov_base = (void*)&pattern[8];
		iov[1].iov_len = pattern_size - 8;
		CALL_AND_CHECK(count, writev(fd, iov, 2), (size_t)count == (pattern_size + 5), "");
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");
		CALL_AND_CHECK(fd, open(file_name3, O_RDONLY), fd >= 0, "file_name=%s", file_name3);
		memset(buf, 0, sizeof(buf));
		/* read "a single line" */
		iov[0].iov_base = buf;
		iov[0].iov_len = 13;
		/* read " line of text" to offset 8 */
		iov[1].iov_base = &buf[8];
		iov[1].iov_len = pattern_size;
		CALL_AND_CHECK(count, readv(fd, iov, 2), (size_t)count == (pattern_size + 5), "");
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");
		printf("content of buffer: \"%s\"\n", buf);
		if (strcmp(buf, pattern) != 0) {
			printf("unexpected content of file\n");
			throw Test_failed();
		} else {
			printf("file content is correct\n");
		}

		/* read directory entries */
		DIR *dir;

		CALL_AND_CHECK(ret, chdir(".."), ret == 0, "dir_name=..");
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

		/* test 'ftruncate()' */
		CALL_AND_CHECK(fd, open(file_name4, O_CREAT | O_WRONLY), fd >= 0, "file_name=%s", file_name4);
		CALL_AND_CHECK(ret, ftruncate(fd, 100), ret == 0, ""); /* increase size */
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");
		CALL_AND_CHECK(ret, stat(file_name4, &stat_buf),
					   (ret == 0) && (stat_buf.st_size == 100),
					   "file_name=%s", file_name4);
		CALL_AND_CHECK(fd, open(file_name4, O_WRONLY), fd >= 0, "file_name=%s", file_name4);
		CALL_AND_CHECK(ret, ftruncate(fd, 10), ret == 0, ""); /* decrease size */
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");
		CALL_AND_CHECK(ret, stat(file_name4, &stat_buf),
					   (ret == 0) && (stat_buf.st_size == 10),
					   "file_name=%s", file_name4);

		/* test 'O_TRUNC' flag */
		CALL_AND_CHECK(fd, open(file_name4, O_WRONLY | O_TRUNC), fd >= 0, "file_name=%s", file_name4);
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");
		CALL_AND_CHECK(ret, stat(file_name4, &stat_buf),
					   (ret == 0) && (stat_buf.st_size == 0),
					   "file_name=%s", file_name4);

		/* test 'fchdir()' */
		CALL_AND_CHECK(fd, open(dir_name, O_RDONLY), fd >= 0, "dir_name=%s", dir_name);
		CALL_AND_CHECK(ret, fchdir(fd), ret == 0, "");
		CALL_AND_CHECK(ret, close(fd), ret == 0, "");
		CALL_AND_CHECK(ret, stat(file_name, &stat_buf), ret == 0, "file_name=%s", file_name);

		/* test 'unlink()' */
		CALL_AND_CHECK(ret, unlink(file_name), (ret == 0), "file_name=%s", file_name);
		CALL_AND_CHECK(ret, stat(file_name, &stat_buf), (ret == -1), "file_name=%s", file_name);
		CALL_AND_CHECK(ret, stat(dir_name2, &stat_buf), (ret == 0), "dir_name=%s", dir_name2);
		CALL_AND_CHECK(ret, rmdir(dir_name2), (ret == 0), "dir_name=%s", dir_name2);
		CALL_AND_CHECK(ret, stat(dir_name2, &stat_buf), (ret == -1), "dir_name=%s", dir_name2);

		/* test symbolic links */
		if ((symlink("/", "symlinks_supported") == 0) || (errno != EPERM)) {
			CALL_AND_CHECK(ret, mkdir("a", 0777), ((ret == 0) || (errno == EEXIST)), "dir_name=%s", "a");
			CALL_AND_CHECK(ret, mkdir("c", 0777), ((ret == 0) || (errno == EEXIST)), "dir_name=%s", "c");
			CALL_AND_CHECK(ret, symlink("../a", "c/d"),
						   ((ret == 0) || (errno == EEXIST)), "dir_name=%s", "/c/d");
			CALL_AND_CHECK(ret, symlink("c", "e"), ((ret == 0) || (errno == EEXIST)), "dir_name=%s", "e");

			CALL_AND_CHECK(fd, open("a/b", O_CREAT | O_WRONLY), fd >= 0, "file_name=%s", "a/b");
			CALL_AND_CHECK(count, write(fd, pattern, pattern_size), (size_t)count == pattern_size, "");
			CALL_AND_CHECK(ret, close(fd), ret == 0, "");

			CALL_AND_CHECK(fd, open("e/d/b", O_RDONLY), fd >= 0, "file_name=%s", "e/d/b");
			CALL_AND_CHECK(count, read(fd, buf, sizeof(buf)), (size_t)count == pattern_size, "");
			CALL_AND_CHECK(ret, close(fd), ret == 0, "");
			printf("content of file: \"%s\"\n", buf);
			if (strcmp(buf, pattern) != 0) {
				printf("unexpected content of file\n");
				throw Test_failed();
			} else {
				printf("file content is correct\n");
			}

			/* test unlink for symbolic links */
			CALL_AND_CHECK(ret, unlink("c/d"), (ret == 0), "symlink=%s", "c/d");
			CALL_AND_CHECK(ret, stat("c/d", &stat_buf), (ret == -1), "symlink=%s", "c/d");
		}

		if (i < (iterations - 1))
			sleep(2);
	}
}


struct Main
{
	Main(Genode::Env &env)
	{
		Genode::Attached_rom_dataspace config_rom { env, "config" };

		Libc::with_libc([&] () {

			test(config_rom.xml());
			test_write_read(config_rom.xml());

			printf("test finished\n");
		});

		env.parent().exit(0);
	}
};


void Libc::Component::construct(Libc::Env &env) { static Main main(env); }
