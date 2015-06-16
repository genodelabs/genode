/*
 * \brief   Kernel backend for execution contexts in userland
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
#include <assert.h>
#include <kernel/thread.h>
#include <kernel/pd.h>
#include <kernel/kernel.h>

using namespace Kernel;


Thread::Thread(unsigned const priority, unsigned const quota,
               char const * const label)
: Thread_base(this), Cpu_job(priority, quota),
  _state(AWAITS_START), _signal_receiver(0),
  _label(label) { cpu_exception = RESET; }


void Thread::exception(unsigned const cpu)
{
	switch (cpu_exception) {
	case SUPERVISOR_CALL:
		_call();
		return;
	case PREFETCH_ABORT:
		_mmu_exception();
		return;
	case DATA_ABORT:
		_mmu_exception();
		return;
	case INTERRUPT_REQUEST:
		_interrupt(cpu);
		return;
	case FAST_INTERRUPT_REQUEST:
		_interrupt(cpu);
		return;
	case UNDEFINED_INSTRUCTION:
		if (_cpu->retry_undefined_instr(&_lazy_state)) { return; }
		PWRN("%s -> %s: undefined instruction at ip=%p",
		     pd_label(), label(), (void*)ip);
		_stop();
		return;
	case RESET:
		return;
	default:
		PWRN("%s -> %s: triggered an unknown exception %lu",
		     pd_label(), label(), (unsigned long)cpu_exception);
		_stop();
		return;
	}
}


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
		/* [18] */ (addr_t Thread::*)&Thread::_fault_pd,
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
	_become_inactive(AWAITS_RESUME);
	if (in_fault(_fault_addr, _fault_writes)) {
		_fault_pd     = (addr_t)_pd->platform_pd();
		_fault_signal = (addr_t)_fault.signal_context();

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
	PERR("unknown MMU exception");
}


void Thread::_call_update_pd()
{
	Pd * const pd = (Pd *) user_arg_1();
	if (Cpu_domain_update::_do_global(pd->asid)) { _pause(); }
}
