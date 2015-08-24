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

void Kernel::Thread::_init() { cpu_exception = RESET; }


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
