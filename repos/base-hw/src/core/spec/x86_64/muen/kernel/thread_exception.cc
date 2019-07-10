/*
 * \brief  Kernel backend for execution contexts in userland
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-04-28
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
	switch (regs->trapno) {
	case Cpu::Context::PAGE_FAULT:
		_mmu_exception();
		return;
	case Cpu::Context::UNDEFINED_INSTRUCTION:
		Genode::raw(*this, ": undefined instruction at ip=", (void*)regs->ip);
		_die();
		return;
	case Cpu::Context::SUPERVISOR_CALL:
		_call();
		return;
	}
	if (regs->trapno >= Cpu::Context::INTERRUPTS_START &&
	    regs->trapno <= Cpu::Context::INTERRUPTS_END) {
		cpu.pic().irq_occurred(regs->trapno);
		_interrupt(cpu.id());
		return;
	}
	Genode::raw(*this, ": triggered unknown exception ", regs->trapno,
	            " with error code ", regs->errcode, " at ip=", (void*)regs->ip);
	_die();
}
