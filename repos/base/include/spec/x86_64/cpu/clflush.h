/*
 * \brief  Helper for flushing cache lines
 * \author Johannes Schlatow
 * \date   2023-09-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_64__CLFLUSH_H_
#define _INCLUDE__SPEC__X86_64__CLFLUSH_H_

namespace Genode {
	inline void clflush(volatile void *addr)
	{
		asm volatile("clflush %0" : "+m" (*(volatile char *)addr));
	}
}

#endif /* _INCLUDE__SPEC__X86_64__CLFLUSH_H_ */
