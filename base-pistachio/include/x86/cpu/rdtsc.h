/*
 * \brief  Read time-stamp counter
 * \author Norman Feske
 * \date   2008-11-29
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__X86__CPU__RDTSC_H_
#define _INCLUDE__X86__CPU__RDTSC_H_

#include <base/clock.h>

namespace Genode {

	static inline cycles_t rdtsc()
	{
		uint32_t lo, hi;
		/* We cannot use "=A", since this would use %rax on x86_64 */
		__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
		return (uint64_t)hi << 32 | lo;
	}
}

#endif /* _INCLUDE__X86__CPU__RDTSC_H_ */
