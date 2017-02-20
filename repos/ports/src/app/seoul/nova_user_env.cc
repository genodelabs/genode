/*
 * \brief  Environment expected by the Vancouver code
 * \author Norman Feske
 * \date   2011-11-18
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <base/thread.h>

/* VMM utils */
#include <vmm/utcb_guard.h>

/* NOVA userland includes */
#include <service/logging.h>
#include <service/memory.h>

enum { verbose_memory_leak = false };


Genode::Lock *printf_lock()
{
	static Genode::Lock inst;
	return &inst;
}


typedef Vmm::Utcb_guard::Utcb_backup Utcb_backup;

static Utcb_backup utcb_backup;


void Logging::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	Genode::Lock::Guard guard(*printf_lock());

	utcb_backup = *(Utcb_backup *)Genode::Thread::myself()->utcb();

	Genode::printf("VMM: ");
	Genode::vprintf(format, list);

	*(Utcb_backup *)Genode::Thread::myself()->utcb() = utcb_backup;

	va_end(list);
}


void Logging::vprintf(const char *format, va_list &ap)
{
	Genode::Lock::Guard guard(*printf_lock());

	utcb_backup = *(Utcb_backup *)Genode::Thread::myself()->utcb();

	Genode::printf("VMM: ");
	Genode::printf(format);
	Genode::error("Logging::vprintf not implemented");

	*(Utcb_backup *)Genode::Thread::myself()->utcb() = utcb_backup;
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

static Genode::Allocator * heap = nullptr;

void heap_init_env(Genode::Heap *h)
{
	heap = h;
}

static void *heap_alloc(size_t size)
{
	void *res = heap->alloc(size);
	if (res)
		return res;

	Genode::error("out of memory");
	Genode::sleep_forever();
}

static void heap_free(void * ptr)
{
	if (heap->need_size_for_free()) {
		Genode::warning("leaking memory");
		return;
	}

	heap->free(ptr, 0);
}


void *operator new[](size_t size)
{
	void * addr = heap_alloc(size);
	if (addr)
		Genode::memset(addr, 0, size);

	return addr;
}


void *operator new[](size_t size, Aligned const alignment)
{
	size_t align = alignment.alignment;
	void *res = heap_alloc(size + align);
	if (res)
		Genode::memset(res, 0, size + align);
	void *aligned_res = (void *)(((Genode::addr_t)res & ~(align - 1)) + align);
	return aligned_res;
}


void *operator new (size_t size)
{
	void * addr = heap_alloc(size);
	if (addr)
		Genode::memset(addr, 0, size);
	return addr;
}


void operator delete[](void *ptr)
{
	if (verbose_memory_leak)
		Genode::warning("delete[] not implemented ", ptr);
}

void operator delete (void * ptr)
{
	heap_free(ptr);
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

/* parameter support */
#include <service/params.h>

Genode::Fifo<Parameter> &Parameter::all_parameters()
{
  static Genode::Fifo<Parameter> _all_parameters;
  return _all_parameters;
}

// EOF
