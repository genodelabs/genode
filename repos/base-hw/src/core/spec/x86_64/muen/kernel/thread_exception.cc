/*
 * \brief  Kernel backend for execution contexts in userland
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-04-28
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread.h>
#include <pic.h>

using namespace Kernel;

void Thread::exception(unsigned const cpu)
{
	switch (trapno) {
	case PAGE_FAULT:
		_mmu_exception();
		return;
	case NO_MATH_COPROC:
		if (_cpu->fpu().fault(*this)) { return; }
		PWRN("%s -> %s: FPU error", pd_label(), label());
		_stop();
		return;
	case UNDEFINED_INSTRUCTION:
		PWRN("%s -> %s: undefined instruction at ip=%p",
			 pd_label(), label(), (void*)ip);
		_stop();
		return;
	case SUPERVISOR_CALL:
		_call();
		return;
	}
	if (trapno >= INTERRUPTS_START && trapno <= INTERRUPTS_END) {
		pic()->irq_occurred(trapno);
		_interrupt(cpu);
		return;
	}
	PWRN("%s -> %s: triggered unknown exception %lu with error code %lu"
		 " at ip=%p", pd_label(), label(), trapno, errcode, (void*)ip);
	_stop();
}
