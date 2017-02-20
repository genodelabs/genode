/*
 * \brief   Kernel cpu context specific implementation
 * \author  Stefan Kalkowski
 * \date    2015-02-11
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <base/log.h>

void Kernel::Cpu_context::_init(size_t const stack_size, addr_t const table)
{
	/*
	 * The stack pointer already contains the stack base address
	 * of all CPU's kernel stacks, on this uni-processor platform
	 * it is sufficient to increase it by the stack's size
	 */
	sp  = sp + stack_size;
	cr3 = Genode::Cpu::Cr3::init(table);
}
