/*
 * \brief  Support for exceptions libsupc++
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2006-07-21
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

extern "C" char __eh_frame_start__[];                  /* from linker script */
extern "C" void __register_frame (const void *begin);  /* from libgcc_eh     */

/*
 * This symbol is set by Genode's dynamic linker (ldso) during binary setup.
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
int (*genode__dl_iterate_phdr) (int (*callback) (void *info, unsigned long size, void *data), void *data) = 0;

extern "C" int dl_iterate_phdr(int (*callback) (void *info, unsigned long size, void *data), void *data) __attribute__((weak));
extern "C" int dl_iterate_phdr(int (*callback) (void *info, unsigned long size, void *data), void *data)
{
	if (!genode__dl_iterate_phdr)
		return -1;

	return genode__dl_iterate_phdr(callback, data);
}

extern "C" void raise()
{
	PDBG("raise called - not implemented\n");
}

void init_exception_handling()
{
//	__register_frame(__eh_frame_start__);
}
