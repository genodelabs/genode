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

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "libc_debug.h"

extern "C" ssize_t readlink(const char *path, char *buf, size_t bufsiz)
{
	/*
	 * During malloc, readlink is called with the path argument "/etc/malloc.conf"
	 */
	if (strcmp("/etc/malloc.conf", path) == 0)
		return -1;

	raw_write_str("readlink called path=\n");
	raw_write_str(path);
	raw_write_str("\n");
	return -1;
}

