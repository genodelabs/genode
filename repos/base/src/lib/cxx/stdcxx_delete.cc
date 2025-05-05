/*
 * \brief  stdcxx delete operators
 * \author Norman Feske
 * \date   2006-05-05
 *
 * The 'delete (void *)' operator gets referenced by compiler generated code,
 * so it must be publicly defined in the 'cxx' library. These compiler
 * generated calls seem to get executed only subsequently to explicit
 * 'delete (void *)' calls in application code, which are not supported by the
 * 'cxx' library, so the 'delete (void *)' implementation in the 'cxx' library
 * does not have to do anything. Applications should use the 'delete (void *)'
 * implementation of the 'stdcxx' library instead. To make this possible, the
 * 'delete (void *)' implementation in the 'cxx' library must be 'weak'.
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>

/* C++ runtime includes */
#include <new>

__attribute__((weak)) void operator delete (void *) noexcept
{
	Genode::error("cxx: operator delete (void *) called - not implemented. "
	              "A working implementation is available in the 'stdcxx' library.");
}

__attribute__((weak)) void operator delete (void *, unsigned long)
{
	Genode::error("cxx: operator delete (void *, unsigned long) called - not implemented. "
	              "A working implementation is available in the 'stdcxx' library.");
}


__attribute__((weak)) void operator delete (void *, unsigned long, std::align_val_t)
{
	Genode::error("cxx: operator delete (void *, unsigned long, std::align_val_t) called - not implemented. "
	              "A working implementation is available in the 'stdcxx' library.");
}

__attribute__((weak)) void operator delete (void *, std::align_val_t) noexcept
{
	Genode::error("cxx: operator delete (void *, std::align_val_t) called - not implemented. "
	              "A working implementation is available in the 'stdcxx' library.");
}
