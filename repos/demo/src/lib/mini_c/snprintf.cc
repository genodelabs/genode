/*
 * \brief  Mini C snprintf()
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/snprintf.h>
#include <stdio.h>

extern "C" int snprintf(char *dst, Genode::size_t dst_len, const char *format, ...)
{
	va_list list;
	va_start(list, format);

	Genode::String_console sc(dst, dst_len);
	sc.vprintf(format, list);

	va_end(list);
	return sc.len();
}
