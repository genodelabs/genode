/*
 * \brief  Environment expected by the Seoul code
 * \author Norman Feske
 * \date   2011-11-18
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <log_session/log_session.h>
#include <base/snprintf.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <base/thread.h>

/* Seoul userland includes */
#include <service/logging.h>
#include <service/memory.h>

enum { verbose_memory_leak = false };

static
void vprintf(const char *format, va_list &args)
{
	using namespace Genode;
	static char buf[Log_session::MAX_STRING_LEN-4];

	String_console sc(buf, sizeof(buf));
	sc.vprintf(format, args);

	int n = sc.len();
	if (0 < n && buf[n-1] == '\n') n--;

	log("VMM: ", Cstring(buf, n));
}

void Logging::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	::vprintf(format, list);

	va_end(list);
}


void Logging::vprintf(const char *format, va_list &ap)
{
	::vprintf(format, ap);
}


void Logging::panic(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	Genode::error("VMM PANIC!");
	::vprintf(format, list);

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


void operator delete[](void *ptr, long unsigned int)
{
	if (verbose_memory_leak)
		Genode::warning("delete[] not implemented ", ptr);
}


void operator delete (void * ptr)
{
	heap_free(ptr);
}


void operator delete(void *ptr, long unsigned int)
{
	heap_free(ptr);
}


void do_exit(char const *msg)
{
	Genode::log("*** ", msg);
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
