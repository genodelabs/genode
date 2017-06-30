/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <base/log.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/pd.h>

using namespace Kernel;


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::MIN, 0)
{
	Cpu_job::cpu(cpu);
	regs->cpu_exception = Cpu::Context::RESET;
	regs->ip = (addr_t)&_main;
	regs->sp = (addr_t)&_stack[stack_size];
	init_thread((addr_t)core_pd()->translation_table(), core_pd()->asid);
	init(true);
}


void Cpu_idle::exception(unsigned const cpu)
{
	switch (regs->cpu_exception) {
	case Cpu::Context::INTERRUPT_REQUEST:      _interrupt(cpu); return;
	case Cpu::Context::FAST_INTERRUPT_REQUEST: _interrupt(cpu); return;
	case Cpu::Context::RESET:                                   return;
	default: Genode::raw("Unknown exception in idle thread"); }
}


extern void * kernel_stack;

void Cpu_idle::proceed(unsigned const cpu)
{
	regs->cpu_exception = (addr_t)&kernel_stack + Cpu::KERNEL_STACK_SIZE * (cpu+1);

	asm volatile("mov  sp, %0        \n"
	             "msr  spsr_cxsf, %1 \n"
	             "mov  lr, %2        \n"
	             "ldm  sp, {r0-r14}^ \n"
	             "subs pc, lr, #0    \n"
	             :: "r" (static_cast<Cpu::Context*>(&*regs)),
	                "r" (regs->cpsr), "r" (regs->ip));
}
