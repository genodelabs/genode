/*
 * \brief  C-library back end
 * \author Norman Feske
 * \date   2008-11-11
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "libc_debug.h"

extern "C" int issetugid(void)
{
	raw_write_str("issetugid called, not yet implemented, returning 1\n");
	return 1;
}
