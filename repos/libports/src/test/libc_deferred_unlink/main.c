/*
 * \brief  Access tmp file after unlink
 * \author Norman Feske
 * \date   2023-12-08
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>

static int dir_entry_exists()
{
	DIR *dir = opendir("/tmp");
	struct dirent *dirent = NULL;
	for (;;) {
		dirent = readdir(dir);
		if (!dirent || (strcmp(dirent->d_name, "test") == 0))
			break;
	}
	closedir(dir);
	return (dirent != NULL);
}


int main(int argc, char **argv)
{
	char const * const path    = "/tmp/test";
	char const * const content = "content of tmp file";

	int const write_fd = open(path, O_RDWR|O_CREAT);
	assert(write_fd >= 0);
	assert(dir_entry_exists());

	(void)unlink(path);
	assert(!dir_entry_exists());

	/* the open 'write_fd' keeps the vfs <ram> from unlinking the file now */
	size_t const num_written_bytes = write(write_fd, content, strlen(content));

	/* open same file for reading before closing the 'write_fd' */
	int const read_fd = open(path, O_RDONLY);
	assert(read_fd >= 0);

	close(write_fd); /* 'read_fd' still references the file */

	char buf[100];
	size_t const num_read_bytes = read(read_fd, buf, sizeof(buf));
	assert(num_read_bytes == num_written_bytes);

	close(read_fd);

	/* since no fd refers to the file any longer, it is phyiscally removed now */
	int const expect_no_fd = open(path, O_RDONLY);
	assert(expect_no_fd == -1);

	{
		FILE *tmp = tmpfile();
		assert(tmp != NULL);
		size_t fwrite_len = fwrite("123", 1, 3, tmp);
		assert(fwrite_len == 3);
		fclose(tmp);
	}

	return 0;
}
