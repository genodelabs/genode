/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/stdint.h>
#include <base/printf.h>
#include <base/thread.h>
#include <util/string.h>

/* Codezero includes */
#include <codezero/syscalls.h>


/****************************
 ** Codezero libl4 support **
 ****************************/

/*
 * Unfortunately, the function 'exregs_print_registers' in 'exregs.c' refers to
 * 'memset'. Because we do not want to link core against a C library, we have to
 * resolve this function here.
 */
extern "C" void *memset(void *s, int c, Genode::size_t n) __attribute__((weak));
extern "C" void *memset(void *s, int c, Genode::size_t n)
{
	return Genode::memset(s, c, n);
}


/*
 * Same problem as for 'memset'. The 'printf' symbol is referenced from
 * 'mutex.c' and 'exregs.c' of Codezero's libl4.
 */
extern "C" int printf(const char *format, ...) __attribute__((weak));
extern "C" int printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);
	Genode::vprintf(format, list);
	va_end(list);
	return 0;
}


/**************************
 ** Startup-code helpers **
 **************************/

namespace Genode { void platform_main_bootstrap(); }


Genode::Native_thread_id main_thread_tid;
Codezero::l4_mutex       main_thread_running_lock;


void Genode::platform_main_bootstrap()
{
	static struct Bootstrap
	{
		Bootstrap()
		{
			Codezero::__l4_init();

			main_thread_tid = Codezero::thread_myself();

			Codezero::l4_mutex_init(&main_thread_running_lock);
			Codezero::l4_mutex_lock(&main_thread_running_lock); /* block on first mutex lock */
		}
	} bootstrap;
}
