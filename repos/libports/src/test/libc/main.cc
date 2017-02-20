/*
 * \brief  LibC test program used during the libC porting process
 * \author Norman Feske
 * \date   2008-10-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/*
 * Mixing Genode headers and libC to see it they collide...
 */

/* Genode includes */
#include <base/env.h>

/* libC includes */
#include <stdio.h>
#include <stdlib.h>

void *addr = 0;

int main(int argc, char **argv)
{
	printf("--- libC test ---\n");

	printf("Does printf work?\n");
	printf("We can find out by printing a floating-point number: %f. How does that work?\n", 1.2345);

	printf("Malloc test\n");
	addr = malloc(1234);
	printf("Malloc returned addr = %p\n", addr);

	printf("--- exit(0) from main ---\n");
	exit(0);
}
