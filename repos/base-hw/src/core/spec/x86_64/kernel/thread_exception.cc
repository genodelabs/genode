/*
 * \brief  Kernel backend for execution contexts in userland
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/thread.h>

using namespace Kernel;


void Thread::exception(Cpu & cpu)
{
	using Genode::Cpu_state;

	switch (regs->trapno) {

	case Cpu_state::PAGE_FAULT:
		_mmu_exception();
		return;

	case Cpu_state::DIVIDE_ERROR:
	case Cpu_state::DEBUG:
	case Cpu_state::BREAKPOINT:
	case Cpu_state::UNDEFINED_INSTRUCTION:
	case Cpu_state::GENERAL_PROTECTION:
		_exception();
		return;

	case Cpu_state::SUPERVISOR_CALL:
		_call();
		return;
	}

	if (regs->trapno >= Cpu_state::INTERRUPTS_START &&
	    regs->trapno <= Cpu_state::INTERRUPTS_END) {
		_interrupt(_user_irq_pool, cpu.id());
		return;
	}

	Genode::raw(*this, ": triggered unknown exception ", regs->trapno,
	            " with error code ", regs->errcode, " at ip=", (void*)regs->ip, " sp=", (void*)regs->sp);

	_die();
}
