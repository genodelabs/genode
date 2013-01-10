/*
 * \brief  Test if the exception mechanism works in hybrid applications
 * \author Christian Prochaska
 * \date   2011-11-22
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

using namespace Genode;

class Test_exception { };

/**
 * Main program
 */
int main(int, char **)
{
	printf("--- lx_hybrid exception test ---\n");

	try {
		printf("Throwing Test_exception\n");
		throw Test_exception();
	} catch(Test_exception) {
		printf("Caught Test_exception\n");
	}

	printf("--- returning from main ---\n");

	return 0;
}
