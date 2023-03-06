/*
 * \brief  Mini C vsnprintf()
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <format/snprintf.h>

extern "C" int vsnprintf(char *dst, Format::size_t dst_len, const char *format, va_list list)
{
	Format::String_console sc(dst, dst_len);
	sc.vprintf(format, list);
	return (int)sc.len();
}
