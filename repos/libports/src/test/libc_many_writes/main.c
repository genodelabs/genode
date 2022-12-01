/*
 * \brief  Libc test stressing the batching of write operations
 * \author Norman Feske
 * \date   2022-12-01
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>


static void print_time(char const *msg)
{
	struct timespec tp;
	bzero(&tp, sizeof(tp));

	int res = clock_gettime(CLOCK_MONOTONIC, &tp);
	if (res != 0) {
		printf("error: clock_gettime failed (%d)\n", res);
		exit(-1);
	}

	printf("%s:  %ld s  %ld ms\n", msg, tp.tv_sec, tp.tv_nsec/1000000);
}


int main(int argc, char **argv)
{
	char const * const path = "/rw/data";

	int const fd = open(path, O_CREAT | O_RDWR);
	if (fd < 0)
		printf("error: creation of file '%s' failed (%d)\n", path, fd);

	print_time("start");

	for (int i = 0; i < 100; i++) {
		printf("write\n");
		unsigned char c = i & 0xffu;
		write(fd, &c, 1);
	}

	print_time("end");

	close(fd);

	printf("exiting\n");
	return 0;
}
