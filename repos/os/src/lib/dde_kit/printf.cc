/*
 * \brief  Formatted output
 * \author Christian Helmuth
 * \date   2008-08-15
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

extern "C" {
#include <dde_kit/printf.h>
}


extern "C" void dde_kit_print(const char *msg)
{
	Genode::printf("%s", msg);
}


extern "C" void dde_kit_printf(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);
}


extern "C" void dde_kit_vprintf(const char *fmt, va_list va)
{
	Genode::vprintf(fmt, va);
}
