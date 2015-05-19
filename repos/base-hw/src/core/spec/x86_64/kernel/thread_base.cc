/*
 * \brief   CPU specific implementations of core
 * \author  Martin Stein
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013, 2015 Genode Labs GmbH
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
	static addr_t Thread::* const _regs[] = {
		/* [0] */ (addr_t Thread::*)&Thread::ip,
		/* [1] */ (addr_t Thread::*)&Thread::sp,
		/* [2] */ (addr_t Thread::*)&Thread::_fault_pd,
		/* [3] */ (addr_t Thread::*)&Thread::_fault_addr,
		/* [4] */ (addr_t Thread::*)&Thread::_fault_writes,
		/* [5] */ (addr_t Thread::*)&Thread::_fault_signal
	};
	return id < sizeof(_regs)/sizeof(_regs[0]) ? _regs[id] : 0;
}


Thread_event Thread::* Thread::_event(unsigned const id) const
{
	static Thread_event Thread::* _events[] = {
		/* [0] */ &Thread::_fault
	};
	return id < sizeof(_events)/sizeof(_events[0]) ? _events[id] : 0;
}


void Thread::_mmu_exception()
{
	_become_inactive(AWAITS_RESUME);
	_fault_pd     = (addr_t)_pd->platform_pd();
	_fault_signal = (addr_t)_fault.signal_context();
	_fault_addr   = Cpu::Cr2::read();

	/**
	 * core should never raise a page-fault,
	 * if this happens print out an error message with debug information
	 */
	if (_pd == Kernel::core_pd())
		PERR("Pagefault in core thread (%s): ip=%p fault=%p",
		     label(), (void*)ip, (void*)_fault_addr);

	_fault.submit();
	return;
}


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
