/*
 * \brief  C-library back end
 * \author Norman Feske
 * \date   2008-11-11
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <base/sleep.h>

/* libc-internal includes */
#include <internal/types.h>

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
		using namespace Libc;

		if (__cleanup)
			(*__cleanup)();

		_exit(status);
	}
}
