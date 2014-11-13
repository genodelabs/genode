/*
 * \brief  Atomic operations for ARM
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \author Martin Stein
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
		int result;
		asm volatile (

			/* compare values */
			"1:\n"
			"ldrex %0, [%1]\n"
			"cmp %0, %2\n"

			/* if not equal, return with result 0 */
			"movne %0, #0\n"
			"bne 2f\n"

			/* if equal, try to override memory value exclusively */
			"strex %0, %3, [%1]\n"
			"cmp %0, #0\n"

			/* if access wasn't exclusive, go back to comparison */
			"bne 1b\n"

			/* if access was exclusive, return with result 1 */
			"mov %0, #1\n"
			"2:\n"
			: "=&r" (result) : "r" (dest), "r" (cmp_val), "r" (new_val)
			: "cc");

		Genode::memory_barrier();
		return result;
	}
}

#endif /* _INCLUDE__SPEC__ARM__CPU__ATOMIC_H_ */
