/*
 * \brief  Trace timestamp
 * \author Josef Soentgen
 * \date   2013-02-12
 *
 * Serialized reading of tsc on x86_32.
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_32__TRACE__TIMESTAMP_H_
#define _INCLUDE__SPEC__X86_32__TRACE__TIMESTAMP_H_

#include <base/fixed_stdint.h>

namespace Genode { namespace Trace {

	typedef uint64_t Timestamp;

	inline Timestamp timestamp()
	{
		uint64_t t;
		__asm__ __volatile__ (
				"pushl %%ebx\n\t"
				"xorl %%eax,%%eax\n\t"
				"cpuid\n\t"
				"popl %%ebx\n\t"
				:
				:
				: "%eax", "%ecx", "%edx"
				);
		__asm__ __volatile__ (
				"rdtsc" : "=A" (t)
				);

		return t;
	}
} }

#endif /* _INCLUDE__SPEC__X86_32__TRACE__TIMESTAMP_H_ */
