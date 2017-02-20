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
 * On x86, the architecture ensures to not reorder writes with older reads,
 * writes to memory with other writes (except in cases that are not relevant
 * for our locks), or read/write instructions with I/O instructions, locked
 * instructions, and serializing instructions. Thus, a compiler memory-barrier
 * is sufficient.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86__CPU__MEMORY_BARRIER_H_
#define _INCLUDE__SPEC__X86__CPU__MEMORY_BARRIER_H_

namespace Genode {

	static inline void memory_barrier()
	{
		asm volatile ("" ::: "memory");
	}
}

#endif /* _INCLUDE__SPEC__X86__CPU__MEMORY_BARRIER_H_ */
