/*
 * \brief  Environment expected by the Vancouver code
 * \author Norman Feske
 * \date   2011-11-18
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/thread.h>

/* NOVA userland includes */
#include <service/logging.h>

enum { verbose_memory_leak = false };


Genode::Lock *printf_lock()
{
	static Genode::Lock inst;
	return &inst;
}


static Genode::Native_utcb utcb_backup;


void Logging::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	Genode::Lock::Guard guard(*printf_lock());

	utcb_backup = *Genode::Thread_base::myself()->utcb();

	Genode::printf("VMM: ");
	Genode::vprintf(format, list);

	*Genode::Thread_base::myself()->utcb() = utcb_backup;

	va_end(list);
}


void Logging::vprintf(const char *format, char *&)
{
	Genode::Lock::Guard guard(*printf_lock());

	utcb_backup = *Genode::Thread_base::myself()->utcb();

	Genode::printf("VMM: ");
	Genode::printf(format);
	PWRN("Logging::vprintf not implemented");

	*Genode::Thread_base::myself()->utcb() = utcb_backup;
}


void Logging::panic(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	Genode::printf("\nVMM PANIC! ");
	Genode::vprintf(format, list);
	Genode::printf("\n");

	va_end(list);

	for (;;)
		Genode::sleep_forever();
}


void *heap_alloc(unsigned int size)
{
	void *res = Genode::env()->heap()->alloc(size);
	if (res)
		return res;

	PERR("out of memory");
	Genode::sleep_forever();
}


void *operator new[](unsigned int size)
{
	void * addr = heap_alloc(size);
	if (addr)
		Genode::memset(addr, 0, size);

	return addr;
}


void *operator new[](unsigned int size, unsigned int align)
{
	void *res = heap_alloc(size + align);
	if (res)
		Genode::memset(res, 0, size + align);
	void *aligned_res = (void *)(((Genode::addr_t)res & ~(align - 1)) + align);
	return aligned_res;
}


void *operator new (unsigned int size)
{
	void * addr = heap_alloc(size);
	if (addr)
		Genode::memset(addr, 0, size);
	return addr;
}


void operator delete[](void *ptr)
{
	if (verbose_memory_leak)
		PWRN("delete[] not implemented");
}


void do_exit(char const *msg)
{
	Genode::printf("*** ");
	Genode::printf(msg);
	Genode::printf("\n");
	Genode::sleep_forever();
}


char __param_table_start;
char __param_table_end;

