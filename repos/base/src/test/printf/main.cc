/*
 * \brief  Testing 'printf()' with negative integer
 * \author Christian Prochaska
 * \date   2012-04-20
 *
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

int main(int argc, char **argv)
{
	/* test that unsupported commands don't crash the printf parser */
	Genode::printf("%#x %s\n", 0x38, "test 1");
	Genode::printf("%#lx %s\n", 0x38L, "test 2");

	Genode::printf("-1 = %d = %ld\n", -1, -1L);

	return 0;
}
