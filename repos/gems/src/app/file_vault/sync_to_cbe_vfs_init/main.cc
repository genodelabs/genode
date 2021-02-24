/*
 * \brief  Helps synchronizing the CBE manager to the CBE-driver initialization
 * \author Martin Stein
 * \date   2021-03-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libC includes */
extern "C" {
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
}

int main(int, char **)
{
	char const *file_path { "/cbe/cbe/current/data" };
	int const file_descriptor = open(file_path, O_RDONLY);
	if (file_descriptor < 0) {
		printf("Error: failed to open file %s\n", file_path);
		exit(-1);
	}
	int const result { fsync(file_descriptor) };
	if (result != 0) {
		printf("Error: fsync on file %s failed\n", file_path);
		exit(-1);
	}
	exit(0);
}
