/*
 * \brief  Core-specific 'printf' implementation
 * \author Norman Feske
 * \date   2010-08-31
 *
 * In contrast to regular Genode processes, which use the platform-
 * independent LOG-session interface as back end of 'printf', core has
 * to rely on a platform-specific back end such as a serial driver or a
 * kernel-debugger function. The platform-specific back end is called
 * 'Core_console'.
 *
 * This file contains the generic glue code between 'printf' and
 * 'Core_console'.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/lock.h>

/* base-internal includes */
#include <base/internal/core_console.h>

using namespace Genode;


/**
 * Synchronized version of the core console
 *
 * This class synchronizes calls of the 'Console::vprintf' function as
 * used by 'printf' and 'vprintf' to prevent multiple printf-using
 * threads within core from interfering with each other.
 */
struct Synchronized_core_console : public Core_console, public Lock
{
	void vprintf(const char *format, va_list list)
	{
		Lock::Guard lock_guard(*this);
		Core_console::vprintf(format, list);
	}
};


/**
 * Return singleton instance of synchronized core console
 */
static Synchronized_core_console &core_console()
{
	static Synchronized_core_console _console;
	return _console;
}


void Genode::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	core_console().vprintf(format, list);

	va_end(list);
}


void Genode::vprintf(const char *format, va_list list)
{
	core_console().vprintf(format, list);
}


