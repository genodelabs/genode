/**
 * \brief  Kernel cpu context specific implementation
 * \author Sebastian Sumpf
 * \date   2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <kernel/cpu.h>

void Kernel::Cpu_context::_init(size_t const stack_size, addr_t const table)
{
	/*
	 * The stack pointer currently contains the base address of the
	 * kernel stack area that contains the kernel stacks of all CPUs. As this
	 * is a uni-processor platform, we merely have to select the first kernel
	 * stack, i.e. increasing sp by the size of one stack.
	 */
	sp = sp + stack_size;
	translation_table(table);
	protection_domain(0);
}
