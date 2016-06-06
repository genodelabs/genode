/*
 * \brief   Backtrace helper utility
 * \date    2015-09-18
 * \author  Christian Prochaska
 * \author  Stefan Kalkowski
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SPEC__ARM__OS__BACKTRACE_H_
#define _INCLUDE__SPEC__ARM__OS__BACKTRACE_H_

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

	asm volatile ("mov %0, %%fp" : "=r"(fp) : :);

	while (fp && *fp) {
		Genode::log(Hex(*fp));
		fp = (addr_t*)*(fp - 1);
	}
}

#endif /* _INCLUDE__SPEC__ARM__OS__BACKTRACE_H_ */
