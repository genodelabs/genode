/*
 * \brief  Test for the Genode console and exception handling
 * \author Norman Feske
 * \date   2009-03-24
 *
 * This program can be started as roottask replacement directly on
 * the OKL4 kernel.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* local includes */
#include "../mini_env.h"

/**
 * Main program
 */
int main()
{
	Genode::printf("Hello, this is Genode's printf.\n");

	try {
		throw 1;
	} catch (...) {
		Genode::printf("Successfully caught an exception.\n");
	}

	return 0;
}
