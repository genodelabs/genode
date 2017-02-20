/*
 * \brief   Backtrace helper utility
 * \date    2015-09-18
 * \author  Christian Prochaska
 * \author  Stefan Kalkowski
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_32__OS__BACKTRACE_H_
#define _INCLUDE__SPEC__X86_32__OS__BACKTRACE_H_

#include <base/stdint.h>
#include <base/log.h>

namespace Genode { void inline backtrace() __attribute__((always_inline)); }

/**
 * Print frame pointer based backtrace
 *
 * To use this function compile your code with the -fno-omit-frame-pointer GCC
 * option.
 */
void inline Genode::backtrace()
{
	Genode::addr_t * fp;

		asm volatile ("movl %%ebp, %0" : "=r"(fp) : :);

		while (fp && *(fp + 1)) {
			Genode::log(Hex(*(fp + 1)));
			fp = (Genode::addr_t*)*fp;
		}
}

#endif /* _INCLUDE__SPEC__X86_32__OS__BACKTRACE_H_ */
