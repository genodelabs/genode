/*
 * \brief  Linux resource path
 * \author Christian Helmuth
 * \date   2011-09-25
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/snprintf.h>

#include <linux_syscalls.h>
#include <linux_rpath.h>


namespace {
	struct Rpath
	{
		char string[32];

		Rpath()
		{
			Genode::snprintf(string, sizeof(string), "/tmp/genode-%d", lx_getuid());
		}
	};
}


char const * lx_rpath()
{
	static Rpath rpath;

	return rpath.string;
}
