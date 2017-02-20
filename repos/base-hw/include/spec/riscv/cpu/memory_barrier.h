/*
 * \brief  Memory barrier
 * \author Sebastian Sumpf
 * \date   2015-06-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RISCV__CPU__MEMORY_BARRIER_H_
#define _INCLUDE__RISCV__CPU__MEMORY_BARRIER_H_

namespace Genode {

	static inline void memory_barrier() {
		asm volatile ("fence" ::: "memory"); }
}

#endif /* _INCLUDE__RISCV__CPU__MEMORY_BARRIER_H_ */
