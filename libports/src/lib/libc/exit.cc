/*
 * \brief  C-library back end
 * \author Norman Feske
 * \date   2008-11-11
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/sleep.h>
#include <base/printf.h>

extern void genode_exit(int status) __attribute__((noreturn));

extern "C" void _exit(int status)
{
	genode_exit(status);
}


extern "C" {

	/* as provided by the original stdlib/exit.c */
	int __isthreaded = 0;

	void (*__cleanup)(void);

	void exit(int status)
	{
		if (status == 4) {
			PDBG("PT: %p %p %p", __builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2));
		}
		if (__cleanup)
			(*__cleanup)();

		_exit(status);
	}
}
