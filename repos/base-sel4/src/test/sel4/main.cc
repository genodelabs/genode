/*
 * \brief  Test for exercising the seL4 kernel interface
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/string.h>

/* seL4 includes */
#include <sel4/arch/functions.h>
#include <sel4/arch/syscalls.h>

int main()
{
	char const *string = "\nMessage printed via the kernel\n";
	for (unsigned i = 0; i < Genode::strlen(string); i++)
		seL4_DebugPutChar(string[i]);

	*(int *)0x1122 = 0;
	return 0;
}
