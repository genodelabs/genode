/*
 * \brief   Debugging support
 * \author  Christian Helmuth
 * \date    2008-08-15
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/sleep.h>

extern "C" {
#include <dde_kit/panic.h>
}

extern "C" void dde_kit_panic(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);
	Genode::printf("\n");

	/* XXX original implementation enters a kernel debugger here */
	Genode::sleep_forever();
}

extern "C" void dde_kit_debug(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);
	Genode::printf("\n");

	/* XXX original implementation enters a kernel debugger here */
	Genode::sleep_forever();
}
