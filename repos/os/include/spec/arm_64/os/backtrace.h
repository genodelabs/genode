/*
 * \brief   Backtrace helper utility
 * \date    2020-01-21
 * \author  Stefan Kalkowski
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__OS__BACKTRACE_H_
#define _INCLUDE__SPEC__ARM_64__OS__BACKTRACE_H_

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
	addr_t * fp;

	asm volatile ("mov %0, x29" : "=r"(fp) ::);

	while (fp) {
		addr_t ip = fp[1];
		fp = (addr_t*) fp[0];
		Genode::log(Hex(ip));
	}
}

#endif /* _INCLUDE__SPEC__ARM_64__OS__BACKTRACE_H_ */

