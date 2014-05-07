/*
 * \brief  Mini C vsnprintf()
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/snprintf.h>

extern "C" int vsnprintf(char *dst, Genode::size_t dst_len, const char *format, va_list list)
{
	Genode::String_console sc(dst, dst_len);
	sc.vprintf(format, list);
	return sc.len();
}
