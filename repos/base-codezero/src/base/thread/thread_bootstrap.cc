/*
 * \brief  Thread bootstrap code
 * \author Christian Prochaska
 * \author Martin Stein
 * \date   2013-02-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/env.h>
#include <util/string.h>

/* Codezero includes */
#include <codezero/syscalls.h>

Genode::Native_thread_id main_thread_tid;
Codezero::l4_mutex       main_thread_running_lock;


/*****************************
 ** Startup library support **
 *****************************/

void prepare_init_main_thread()
{
	/* initialize codezero environment */
	Codezero::__l4_init();

	/* provide kernel identification of thread through temporary environment */
	main_thread_tid = Codezero::thread_myself();
}

void prepare_reinit_main_thread() { prepare_init_main_thread(); }


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


/*****************
 ** Thread_base **
 *****************/

void Genode::Thread_base::_thread_bootstrap()
{
	Codezero::l4_mutex_init(utcb()->running_lock());
	Codezero::l4_mutex_lock(utcb()->running_lock()); /* block on first mutex lock */
}


void Genode::Thread_base::_init_platform_thread(Type type)
{
	if (type == NORMAL) { return; }

	/* adjust values whose computation differs for a main thread */
	_tid.l4id = main_thread_tid;
	_thread_cap = Genode::env()->parent()->main_thread_cap();

	/* get first mutex lock (normally done by _thread_bootstrap) */
	Codezero::l4_mutex_init(utcb()->running_lock());
	Codezero::l4_mutex_lock(utcb()->running_lock());
}
