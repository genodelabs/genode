/*
 * \brief   Printf wrappers for Genode
 * \date    2008-07-24
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GENODE_PRINTF_H_
#define _GENODE_PRINTF_H_

#include <base/printf.h>

inline int printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	Genode::vprintf(format, list);

	va_end(list);
	return 0;
}

#endif
