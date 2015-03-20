/*
 * \brief  Kernel backend for execution contexts in userland
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread.h>

using namespace Kernel;

Thread::Thread(unsigned const priority, unsigned const quota,
               char const * const label)
:
	Thread_base(this), Cpu_job(priority, quota), _state(AWAITS_START), _pd(0),
	_utcb_phys(0), _signal_receiver(0), _label(label) { }


void Thread::exception(unsigned const cpu)
{
	switch (trapno) {
	case PAGE_FAULT:
		_mmu_exception();
		return;
	case NO_MATH_COPROC:
		if (_cpu->retry_fpu_instr(&_lazy_state)) { return; }
		PWRN("%s -> %s: FPU error", pd_label(), label());
		_stop();
		return;
	case SUPERVISOR_CALL:
		_call();
		return;
	}
	if (trapno >= INTERRUPTS_START && trapno <= INTERRUPTS_END) {
		_interrupt(cpu);
		return;
	}
	PWRN("%s -> %s: triggered unknown exception %lu with error code %lu",
	     pd_label(), label(), trapno, errcode);
	_stop();
}
