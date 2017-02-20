/*
 * \brief  Atomic operations for ARM
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2007-04-28
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__CPU__ATOMIC_H_
#define _INCLUDE__SPEC__ARM__CPU__ATOMIC_H_

#include <cpu/memory_barrier.h>

namespace Genode {

	/**
	 * Atomic compare and exchange
	 *
	 * This function compares the value at dest with cmp_val.
	 * If both values are equal, dest is set to new_val. If
	 * both values are different, the value at dest remains
	 * unchanged.
	 *
	 * Note, that cmpxchg() represents a memory barrier.
	 *
	 * \return  1 if the value was successfully changed to new_val,
	 *          0 if cmp_val and the value at dest differ.
	 */
	inline int cmpxchg(volatile int *dest, int cmp_val, int new_val)
	{
		unsigned long equal, not_exclusive;

		__asm__ __volatile__(
			"@ cmpxchg\n"
			"  1:                  \n"
			"  ldrex   %0, [%2]    \n"
			"  cmp     %0, %3      \n"
			"  bne     2f          \n"
			"  strexeq %0, %4, [%2]\n"
			"  teqeq   %0, #0      \n"
			"  bne     1b          \n"
			"  moveq   %1, #1      \n"
			"  2:                  \n"
			"  movne   %1, #0      \n"
			: "=&r" (not_exclusive), "=&r" (equal)
			: "r" (dest), "r" (cmp_val), "r" (new_val)
			: "cc");
		Genode::memory_barrier();
		return equal && !not_exclusive;
	}
}

#endif /* _INCLUDE__SPEC__ARM__CPU__ATOMIC_H_ */
