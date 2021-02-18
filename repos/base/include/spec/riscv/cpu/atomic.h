/*
 * \brief  Atomic operations for RISCV
 * \author Sebastian Sumpf
 * \date   2015-06-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RISCV__CPU__ATOMIC_H_
#define _INCLUDE__RISCV__CPU__ATOMIC_H_

/* Genode includes */
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
		int old_val;

		__asm__ __volatile__(
			"  1:                \n"
			"  lr.w %0, (%1)     \n"
			"  bne  %0, %2, 2f   \n"
			"  sc.w %0, %3, (%1) \n"
			"  bnez %0, 1b       \n"
			"  mv   %0, %2       \n"
			"  2:                \n"
			: "=&r" (old_val)
			: "r" (dest), "r" (cmp_val), "r" (new_val)
			: "memory");

		Genode::memory_barrier();
		return old_val == cmp_val ? 1 : 0;
	}
}

#endif /* _INCLUDE__RISCV__CPU__ATOMIC_H_ */
