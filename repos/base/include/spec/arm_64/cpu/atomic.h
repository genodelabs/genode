/*
 * \brief  Atomic operations for ARM 64-bit.
 * \author Stefan Kalkowski
 * \date   2019-03-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__CPU__ATOMIC_H_
#define _INCLUDE__SPEC__ARM_64__CPU__ATOMIC_H_

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
		int fail, old_val = 0;

		do {
			asm volatile(
				"ldxr %w1, %2      \n"
				"mov  %w0, #0      \n"
				"cmp  %w1, %w3     \n"
				"b.ne 1f           \n"
				"stxr %w0, %w4, %2 \n"
				"1:                \n"
				: "=&r" (fail), "=&r" (old_val), "+Q" (*dest)
				:  "r" (cmp_val), "r" (new_val)
				: "cc");
		} while (fail);

		Genode::memory_barrier();
		return (old_val != cmp_val) ? 0 : 1;
	}
}

#endif /* _INCLUDE__SPEC__ARM_64__CPU__ATOMIC_H_ */
