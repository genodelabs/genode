/*
 * \brief  LibC counter test - sink
 * \author Christian Helmuth
 * \date   2017-01-31
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define min(a,b) (((a) < (b)) ? (a) : (b))

enum { MAX_COUNT = 10 };


int main(int argc, char **argv)
{
	fprintf(stderr, "--- counter sink started ---\n");

	static char buf[32];

	/* stdin/stdout are connected to the source */
	for (unsigned i = 0; i < MAX_COUNT; ++i) {
		int nbytes = 0;

		/* XXX busy loop - read does not block */
		unsigned retry = 0;
		while (0 == (nbytes = read(0, buf, sizeof(buf)))) ++retry;
		fprintf(stderr, "nbytes=%d retry=%u\n", nbytes, retry);
		buf[31] = 0;
		fprintf(stderr, "buf=\"%s\"\n", buf);
	}

	fprintf(stderr, "--- counter sink terminates ---\n");
}
