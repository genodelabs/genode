/*
 * \brief  Atomic Userland operations for Microblaze
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU__ATOMIC_H_
#define _INCLUDE__CPU__ATOMIC_H_

#include <kernel/syscalls.h>

namespace Genode {

	extern void* const _atomic_cmpxchg;


	/**
	 * Executes compare and exchange as atomic operation
	 *
	 * This function compares the value at dest with cmp_val.
	 * If both values are equal, dest is set to new_val. If
	 * both values are different, the value at dest remains
	 * unchanged.
	 *
	 * \return  1 if the value was successfully changed to new_val,
	 *          0 if cmp_val and the value at dest differ.
	 */
	inline int cmpxchg(volatile int *dest,
	                   unsigned int cmp_val,
	                   unsigned int new_val)
	{
		int result = 0;
		unsigned int r15_buf = 0;

		/**
		 * r27-r30 are arguments/return-values
		 * for _atomic_cmpxchg in r31 kernel denotes if
		 * interrupt has occured while executing atomic code
		 */
		asm volatile ("lwi r30, %[dest]            \n"
		              "lwi r29, %[cmp_val]         \n"
		              "lwi r28, %[new_val]         \n"
		              "lwi r27, %[dest_val]        \n"
		              "or r31, r0, r0              \n"
		              "swi r15, %[r15_buf]         \n"
		              "bralid r15, _atomic_cmpxchg \n"
		              "or r0, r0, r0               \n"
		              "lwi r15, %[r15_buf]         \n"
		              "swi r28, %[result]            "
		              :
		                [result]   "=m" (result),
		                [r15_buf]  "+m" (r15_buf),
		                [dest]     "+m" (dest),
		                [cmp_val]  "+m" (cmp_val),
		                [new_val]  "+m" (new_val),
		                [dest_val] "+m" (*dest)
		              :: "r31", "r30", "r29", "r28", "r27", "memory");

		return result;
	}
}


#endif /* _INCLUDE__CPU__ATOMIC_H_ */
