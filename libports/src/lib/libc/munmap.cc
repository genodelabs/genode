/*
 * \brief  C-library back end
 * \author Norman Feske
 * \date   2008-11-11
 */

/*
 * Copyright (C) 2008-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <sys/mman.h>

#include "libc_debug.h"

extern "C" int munmap(void *start, size_t length)
{
	raw_write_str("munmap called, not yet implemented!\n");
	return 0;
}
