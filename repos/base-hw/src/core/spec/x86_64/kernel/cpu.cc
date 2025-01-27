/*
 * \brief  Class for kernel data that is needed to manage a specific CPU
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
#include <hw/memory_consts.h>


void Kernel::Cpu::_arch_init()
{
	gdt.init((addr_t)&tss);
	Idt::init();
	Tss::init();

	/* set interrupt stack pointer to kernel context stack minus FPU state size */
	tss.ist[0] = stack_start() - Fpu_context::SIZE;

	_pic.init();
	_timer.init();
	_ipi_irq.init();

	/* enable timer interrupt */
	_pic.unmask(_timer.interrupt_id(), id());
}


[[noreturn]] void Kernel::Cpu::panic(Genode::Cpu_state &state)
{
	using namespace Genode;
	using Cs = Genode::Cpu_state;

	const char *reason = "unknown";

	switch (state.trapno) {
	case Cs::PAGE_FAULT:            reason = "page-fault";            break;
	case Cs::UNDEFINED_INSTRUCTION: reason = "undefined instruction"; break;
	case Cs::SUPERVISOR_CALL:       reason = "system-call";           break;
	default:
		if (state.trapno >= Cs::INTERRUPTS_START &&
		    state.trapno <= Cs::INTERRUPTS_END)
			reason = "interrupt";
	};

	log("");
	log("Kernel panic on CPU ", Cpu::executing_id());
	log("Exception reason is ", reason, " (trapno=", state.trapno, ")");
	log("");
	log("Register dump:");
	log("ip     = ", Hex(state.ip));
	log("sp     = ", Hex(state.sp));
	log("cs     = ", Hex(state.cs));
	log("ss     = ", Hex(state.ss));
	log("eflags = ", Hex(state.eflags));
	log("rax    = ", Hex(state.rax));
	log("rbx    = ", Hex(state.rbx));
	log("rcx    = ", Hex(state.rcx));
	log("rdx    = ", Hex(state.rdx));
	log("rdi    = ", Hex(state.rdi));
	log("rsi    = ", Hex(state.rsi));
	log("rbp    = ", Hex(state.rbp));
	log("CR2    = ", Hex(Cpu::Cr2::read()));
	log("CR3    = ", Hex(Cpu::Cr3::read()));

	while (true) asm volatile("hlt");
}
