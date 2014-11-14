/*
 * \brief  Memory barrier
 * \author Martin Stein
 * \date   2014-11-12
 *
 * The memory barrier prevents memory accesses from being reordered in such a
 * way that accesses to the guarded resource get outside the guarded stage. As
 * cmpxchg() defines the start of the guarded stage it also represents an
 * effective memory barrier.
 *
 * Note, we assume a compiler-memory barrier is sufficient on ARMv5.
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM_V5__CPU__MEMORY_BARRIER_H_
#define _INCLUDE__ARM_V5__CPU__MEMORY_BARRIER_H_

namespace Genode {

	static inline void memory_barrier()
	{
		asm volatile ("" ::: "memory");
	}
}

#endif /* _INCLUDE__ARM_V5__CPU__MEMORY_BARRIER_H_ */
