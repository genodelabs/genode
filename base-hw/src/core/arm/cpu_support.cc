/*
 * \brief   CPU specific implementations of core
 * \author  Martin Stein
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

using namespace Kernel;


/********************************
 ** Kernel::Thread_cpu_support **
 ********************************/

Thread_cpu_support::Thread_cpu_support(Thread * const t)
:
	_fault(t),
	_fault_tlb(0),
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
		/* [0]  */ (addr_t Thread::*)&Thread::r0,
		/* [1]  */ (addr_t Thread::*)&Thread::r1,
		/* [2]  */ (addr_t Thread::*)&Thread::r2,
		/* [3]  */ (addr_t Thread::*)&Thread::r3,
		/* [4]  */ (addr_t Thread::*)&Thread::r4,
		/* [5]  */ (addr_t Thread::*)&Thread::r5,
		/* [6]  */ (addr_t Thread::*)&Thread::r6,
		/* [7]  */ (addr_t Thread::*)&Thread::r7,
		/* [8]  */ (addr_t Thread::*)&Thread::r8,
		/* [9]  */ (addr_t Thread::*)&Thread::r9,
		/* [10] */ (addr_t Thread::*)&Thread::r10,
		/* [11] */ (addr_t Thread::*)&Thread::r11,
		/* [12] */ (addr_t Thread::*)&Thread::r12,
		/* [13] */ (addr_t Thread::*)&Thread::sp,
		/* [14] */ (addr_t Thread::*)&Thread::lr,
		/* [15] */ (addr_t Thread::*)&Thread::ip,
		/* [16] */ (addr_t Thread::*)&Thread::cpsr,
		/* [17] */ (addr_t Thread::*)&Thread::cpu_exception,
		/* [18] */ (addr_t Thread::*)&Thread::_fault_tlb,
		/* [19] */ (addr_t Thread::*)&Thread::_fault_addr,
		/* [20] */ (addr_t Thread::*)&Thread::_fault_writes,
		/* [21] */ (addr_t Thread::*)&Thread::_fault_signal
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
	_unschedule(AWAITS_RESUME);
	if (in_fault(_fault_addr, _fault_writes)) {
		_fault_tlb    = (addr_t)_pd->tlb();
		_fault_signal = _fault.signal_context_id();
		_fault.submit();
		return;
	}
	PERR("unknown MMU exception");
}


/*************************
 ** Kernel::Cpu_context **
 *************************/

void Kernel::Cpu_context::_init(size_t const stack_size) { r12 = stack_size; }


/*************************
 ** CPU-state utilities **
 *************************/

typedef Thread_reg_id Reg_id;

static addr_t const _cpu_state_regs[] = {
	Reg_id::R0,   Reg_id::R1,   Reg_id::R2,  Reg_id::R3, Reg_id::R4,
	Reg_id::R5,   Reg_id::R6,   Reg_id::R7,  Reg_id::R8, Reg_id::R9,
	Reg_id::R10,  Reg_id::R11,  Reg_id::R12, Reg_id::SP, Reg_id::LR,
	Reg_id::IP,   Reg_id::CPSR, Reg_id::CPU_EXCEPTION };

addr_t const * cpu_state_regs() { return _cpu_state_regs; }


size_t cpu_state_regs_length()
{
	return sizeof(_cpu_state_regs)/sizeof(_cpu_state_regs[0]);
}