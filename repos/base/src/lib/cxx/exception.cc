/*
 * \brief  Support for exceptions libsupc++
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2006-07-21
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libsupc++ includes */
#include <cxxabi.h>
#include <exception>

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* base-internal includes */
#include <base/internal/globals.h>

extern "C" char __eh_frame_start__[];                  /* from linker script */
extern "C" void __register_frame (const void *begin);  /* from libgcc_eh     */
extern "C" char *__cxa_demangle(const char *mangled_name,
                                char *output_buffer,
                                size_t *length,
                                int *status);
extern "C" void free(void *ptr);

/*
 * This symbol is overwritten by Genode's dynamic linker (ld.lib.so).
 * After setup, the symbol will point to the actual implementation of
 * 'dl_iterate_phdr', which is located within the linker. 'dl_iterate_phdr'
 * iterates through all (linker loaded) binaries and shared libraries. This
 * function has to be implemented in order to support C++ exceptions within
 * shared libraries.
 * Return values of dl_iterate_phdr (gcc 4.2.4):
 *   < 0 = error
 *     0 = continue program header iteration
 *   > 0 = stop iteration (no errors occured)
 *
 * See also: man dl_iterate_phdr
 */
extern "C" int dl_iterate_phdr(int (*callback) (void *info, unsigned long size,
                                                void *data), void *data) __attribute__((weak));
extern "C" int dl_iterate_phdr(int (*) (void *, unsigned long, void *), void *) {
	return -1; }


static void terminate_handler()
{
	std::type_info *t = __cxxabiv1::__cxa_current_exception_type();

	if (!t)
		return;

	char *demangled_name = __cxa_demangle(t->name(), nullptr, nullptr, nullptr);
	if (demangled_name) {
		Genode::error("Uncaught exception of type "
		              "'", Genode::Cstring(demangled_name), "'");
		free(demangled_name);
	} else {
		Genode::error("Uncaught exception of type '", t->name(), "' "
		              "(use 'c++filt -t' to demangle)");
	}
}


void Genode::init_exception_handling(Ram_allocator &ram, Region_map &rm)
{
	init_cxx_heap(ram, rm);

	__register_frame(__eh_frame_start__);

	std::set_terminate(terminate_handler);

	/*
	 * Trigger first exception. This step has two purposes.
	 * First, it enables us to detect problems related to exception handling as
	 * early as possible. If there are problems with the C++ support library,
	 * it is much easier to debug them at this early stage. Otherwise problems
	 * with half-working exception handling cause subtle failures that are hard
	 * to interpret.
	 *
	 * Second, the C++ support library allocates data structures lazily on the
	 * first occurrence of an exception. In some corner cases, this allocation
	 * consumes several KB of stack. This is usually not a problem when the
	 * first exception is triggered from the main thread but it becomes an
	 * issue when the first exception is thrown from the stack of a thread with
	 * a specially tailored (and otherwise sufficient) stack size. By throwing
	 * an exception here, we mitigate this issue by eagerly performing those
	 * allocations.
	 */
	try { throw 1; } catch (...) { }
}
