/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM___MAIN_HELPER_H_
#define _PLATFORM___MAIN_HELPER_H_

#include <base/printf.h>

/* make Codezero includes happy */
extern "C" char *strncpy(char *dest, const char *src, Genode::size_t n);
extern "C" void *memcpy(void *dest, const void *src, Genode::size_t n);

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


Genode::Native_thread_id main_thread_tid;
Codezero::l4_mutex       main_thread_running_lock;


static void main_thread_bootstrap()
{
	Codezero::__l4_init();

	main_thread_tid = Codezero::thread_myself();

	Codezero::l4_mutex_init(&main_thread_running_lock);
	Codezero::l4_mutex_lock(&main_thread_running_lock); /* block on first mutex lock */
}

#endif /* _PLATFORM___MAIN_HELPER_H_ */
