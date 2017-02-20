/*
 * \brief  Trace timestamp
 * \author Josef Soentgen
 * \date   2013-02-12
 *
 * Serialized reading of tsc on x86_64.
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_64__TRACE__TIMESTAMP_H_
#define _INCLUDE__SPEC__X86_64__TRACE__TIMESTAMP_H_

#include <base/fixed_stdint.h>

namespace Genode { namespace Trace {

	typedef uint64_t Timestamp;

	inline Timestamp timestamp()  __attribute((always_inline));
	inline Timestamp timestamp()
	{
		uint32_t lo, hi;
		__asm__ __volatile__ (
				"xorl %%eax,%%eax\n\t"
				"cpuid\n\t"
				:
				:
				: "%rax", "%rbx", "%rcx", "%rdx"
				);
		__asm__ __volatile__ (
				"rdtsc" : "=a" (lo), "=d" (hi)
				);

		return (uint64_t)hi << 32 | lo;
	}
} }

#endif /* _INCLUDE__SPEC__X86_64__TRACE__TIMESTAMP_H_ */
