/*
 * \brief  LibC counter test - source
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


enum { MAX_COUNT = 10 };


int main(int argc, char **argv)
{
	fprintf(stderr, "--- counter source started ---\n");

	/* idle some time, so the sink has a chance to block ;-) */
	sleep(2);

	static char buf[32];

	/* stdin/stdout are connected to the source */
	for (unsigned i = 0; i < MAX_COUNT; ++i) {
		int nbytes = snprintf(buf, sizeof(buf), "%u.", i);
		nbytes = write(1, buf, nbytes);
		fprintf(stderr, "nbytes=%d\n", nbytes);
		sleep(1);
	}

	fprintf(stderr, "--- counter source terminates ---\n");
}
