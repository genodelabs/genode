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
 * On ARM, the architectural memory model allows not only that memory accesses
 * take local effect in another order as their program order but also that
 * different observers (components that can access memory like data-busses,
 * TLBs and branch predictors) observe these effects each in another order.
 * Thus, achieving a correct program order via a compiler memory-barrier isn't
 * sufficient for a correct observation order. An additional architectural
 * preservation of the memory barrier is needed.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_V7__CPU__MEMORY_BARRIER_H_
#define _INCLUDE__SPEC__ARM_V7__CPU__MEMORY_BARRIER_H_

namespace Genode {

	static inline void memory_barrier()
	{
		asm volatile ("dmb" ::: "memory");
	}
}

#endif /* _INCLUDE__SPEC__ARM_V7__CPU__MEMORY_BARRIER_H_ */
