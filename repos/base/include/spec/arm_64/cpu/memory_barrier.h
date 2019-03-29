/*
 * \brief  Memory barrier
 * \author Stefan Kalkowski
 * \date   2019-03-25
 *
 * The memory barrier prevents memory accesses from being reordered in such a
 * way that accesses to the guarded resource get outside the guarded stage. As
 * cmpxchg() defines the start of the guarded stage it also represents an
 * effective memory barrier.
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__CPU__MEMORY_BARRIER_H_
#define _INCLUDE__SPEC__ARM_64__CPU__MEMORY_BARRIER_H_

namespace Genode {

	static inline void memory_barrier()
	{
		/*
		 * Be conversative for the time-being and synchronize with all level
		 */
		asm volatile ("dsb #15" ::: "memory");
	}
}

#endif /* _INCLUDE__SPEC__ARM_64__CPU__MEMORY_BARRIER_H_ */

