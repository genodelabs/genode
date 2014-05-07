/*
 * \brief  Genode::printf back-end for stdio
 * \author Norman Feske
 * \date   2009-10-06
 *
 * This library can be used by unit test executed on the host platform to
 * direct output from the Genode framework to stdout.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <stdio.h>
#include <base/printf.h>


void Genode::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	::vprintf(format, list);

	va_end(list);
}


void Genode::vprintf(const char *format, va_list list)
{
	::vprintf(format, list);
}
