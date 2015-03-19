/*
 * \brief   CPU specific implementations of core
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
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


/********************
 ** Kernel::Thread **
 ********************/

addr_t Thread::* Thread::_reg(addr_t const id) const
{
	PDBG("not implemented");
	return 0UL;
}


Thread_event Thread::* Thread::_event(unsigned const id) const
{
	PDBG("not implemented");
	return nullptr;
}


void Thread::_mmu_exception()
{
	PDBG("not implemented");
}


/*************************
 ** Kernel::Cpu_context **
 *************************/

void Kernel::Cpu_context::_init(size_t const stack_size, addr_t const table)
{ }


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
