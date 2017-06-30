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

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/pd.h>

using namespace Kernel;


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::MIN, 0)
{
	Cpu::Gdt::init();
	Cpu_job::cpu(cpu);
	regs->ip = (addr_t)&_main;
	regs->sp = (addr_t)&_stack[stack_size];
	regs->cs = 0x8;
	regs->ss = 0x10;
	regs->init((addr_t)core_pd()->translation_table(), true);
}

extern void * __tss_client_context_ptr;

void Cpu_idle::proceed(unsigned const)
{
	void * * tss_stack_ptr = (&__tss_client_context_ptr);
	*tss_stack_ptr = &regs->cr3;

	asm volatile("mov  %0, %%rsp  \n"
	             "popq %%r8       \n"
	             "popq %%r9       \n"
	             "popq %%r10      \n"
	             "popq %%r11      \n"
	             "popq %%r12      \n"
	             "popq %%r13      \n"
	             "popq %%r14      \n"
	             "popq %%r15      \n"
	             "popq %%rax      \n"
	             "popq %%rbx      \n"
	             "popq %%rcx      \n"
	             "popq %%rdx      \n"
	             "popq %%rdi      \n"
	             "popq %%rsi      \n"
	             "popq %%rbp      \n"
	             "add  $16, %%rsp \n"
	             "iretq           \n"
	             :: "r" (&regs->r8));
}


void Kernel::Cpu::init(Pic &pic)
{
	Idt::init();
	Tss::init();

	Timer::init_cpu_local();

	fpu().init();

	/* enable timer interrupt */
	unsigned const cpu = Cpu::executing_id();
	pic.unmask(_timer.interrupt_id(), cpu);
}


void Cpu_domain_update::_domain_update() { }
