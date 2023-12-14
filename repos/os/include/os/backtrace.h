/*
 * \brief   Frame-pointer-based backtrace utility
 * \author  Christian Helmuth
 * \date    2023-12-14
 *
 * To use this utility compile your code with the -fno-omit-frame-pointer GCC
 * option.
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__BACKTRACE_H_
#define _INCLUDE__OS__BACKTRACE_H_

#include <base/stdint.h>
#include <base/thread.h>
#include <base/log.h>
#include <util/formatted_output.h>


namespace Genode {
	void for_each_return_address(auto const &);
	void for_each_return_address(Const_byte_range_ptr const &, auto const &);

	struct Backtrace;

	void inline backtrace() __attribute__((always_inline));
}

#include <os/for_each_return_address.h> /* for_each_return_address(fn, stack) */


/**
 * Walk backtrace and call fn() per step
 *
 * The walk is limited to the memory of the current thread's stack to prevent
 * access outside of mapped memory regions. fn() is passed a pointer to the
 * stack location of the return address.
 */
void Genode::for_each_return_address(auto const &fn)
{
	Thread::Stack_info   const si    { Thread::mystack() };
	Const_byte_range_ptr const stack { (char const *)si.base, si.top - si.base };

	for_each_return_address(stack, fn);
}


/**
 * Printable backtrace for Genode::log(), Genode::trace(), etc.
 */
struct Genode::Backtrace
{
	void print(Output &out) const
	{
		using Genode::print;

		print(out, "backtrace \"", Thread::myself()->name(), "\"");

		struct Addr : Hex { Addr(void *v) : Hex((addr_t)v, OMIT_PREFIX) { } };

		unsigned width = 0;
		for_each_return_address([&] (void **p) {
			width = max(width, printed_length(Addr(*p)));
		});
		if (!width) {
			print(out, "\n  <no return address found>");
			return;
		}

		for_each_return_address([&] (void **p) {
			print(out, "\n  ", Addr(p), "  ", Right_aligned(width, Addr(*p)));
		});
	}
};


/**
 * Print backtrace via Genode::log()
 */
void inline Genode::backtrace()
{
	Genode::log(Backtrace());
}

#endif /* _INCLUDE__OS__BACKTRACE_H_ */
