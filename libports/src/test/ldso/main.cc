/*
 * \brief  ldso test program
 * \author Sebastian Sumpf
 * \date   2009-11-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <rom_session/connection.h>
#include <base/env.h>
using namespace Genode;

/* shared-lib includes */
#include "test-ldso.h"

namespace Libc {
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
}

static void __raise_exception(void)
{
	throw 666;
}

extern void __ldso_raise_exception();

int main(int argc, char **argv)
{
	printf("\nStatic Geekings!\n");
	printf(  "================\n");


	int fd = 0;
	char buf[2];
	printf("Test read\n");
	Libc::read(fd, buf, 2);

	printf("Static object in funcion ... ");
	Link::static_function_object();

	printf("Shared library function call ...");
	Link::dynamic_link_test();

	printf("Exception during RPC: ");
	try {
		Rom_connection rom("__not_found__");
	}
	catch (...) { printf("good\n"); }

	printf("Exception in dynamic binary ... ");
	try {
		__raise_exception();
	}
	catch (...) {
		printf("good (binary)\n");
	}

	printf("Exception inter-shared library ... ");
	try {
		Link::raise_exception();
	}
	catch (Rm_session::Region_conflict) {
		printf("good (inter)\n");
	}
	
	printf("Exception from LDSO with explicit catch ... ");
	try {
		__ldso_raise_exception();
	} catch (Genode::Exception) {
		printf("good\n");
	}

	/* test libc */
	int i = Libc::abs(-10);
	printf("Libc test: abs(-10): %d\n", i);
	printf(  "================\n\n");

	return 0;
}
