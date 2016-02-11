/*
 * \brief   CPU specific implementations of core
 * \author  Sebastian Sumpf
 * \date    2015-06-02
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread.h>
#include <kernel/pd.h>
#include <kernel/kernel.h>

using namespace Kernel;


/*************************
 ** Kernel::Thread_base **
 *************************/

Thread_base::Thread_base(Thread * const t)
:
	_fault(t),
	_fault_pd(0),
	_fault_addr(0),
	_fault_writes(0),
	_fault_signal(0)
{ }


/*************************
 ** Kernel::Cpu_context **
 *************************/

void Kernel::Cpu_context::_init(size_t const stack_size, addr_t const table)
{
	/*
	 * the stack pointer already contains the stack base address
	 * of all CPU's kernel stacks, on this uni-processor platform
	 * it is sufficient to increase it by the stack's size
	 */
	sp = sp + stack_size;
}


/*************************
 ** CPU-state utilities **
 *************************/

typedef Thread_reg_id Reg_id;

static addr_t const _cpu_state_regs[] = { };

addr_t const * cpu_state_regs() { return _cpu_state_regs; }


size_t cpu_state_regs_length()
{
	return sizeof(_cpu_state_regs)/sizeof(_cpu_state_regs[0]);
}
