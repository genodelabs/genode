/*
 * \brief  Atomic operations for x86
 * \author Norman Feske
 * \date   2006-07-26
 *
 * Based on l4util/include/ARCH-x86/atomic_arch.h.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86__CPU__ATOMIC_H_
#define _INCLUDE__SPEC__X86__CPU__ATOMIC_H_

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
		int tmp;

		__asm__ __volatile__
		(
		 "lock cmpxchgl %1, %3 \n\t"
		 :
		 "=a" (tmp)      /* 0 EAX, return val */
		 :
		 "r"  (new_val), /* 1 reg, new value */
		 "0"  (cmp_val), /* 2 EAX, compare value */
		 "m"  (*dest)    /* 3 mem, destination operand */
		 :
		 "memory", "cc"
		);

		return tmp == cmp_val;
	}
}

#endif /* _INCLUDE__SPEC__X86__CPU__ATOMIC_H_ */
