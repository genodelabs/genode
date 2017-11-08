/*
 * \brief   Kernel back-end for execution contexts in userland
 * \author  Martin Stein
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/thread.h>
#include <kernel/pd.h>


void Kernel::Thread::_call_update_data_region() { }


void Kernel::Thread::_call_update_instr_region() { }


void Kernel::Thread::_call_update_pd() {
	Genode::Cpu::Cr3::write(Genode::Cpu::Cr3::read());
}


extern void * __tss_client_context_ptr;

void Kernel::Thread::proceed(Cpu & cpu)
{
	void * * tss_stack_ptr = (&__tss_client_context_ptr);
	*tss_stack_ptr = (void*)((addr_t)&*regs + sizeof(Genode::Cpu_state));

	cpu.switch_to(*regs, pd()->mmu_regs);

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
	             "iretq           \n" :: "r" (&regs->r8));
}


void Kernel::Thread::user_arg_0(Kernel::Call_arg const arg) { regs->rdi = arg; }
void Kernel::Thread::user_arg_1(Kernel::Call_arg const arg) { regs->rsi = arg; }
void Kernel::Thread::user_arg_2(Kernel::Call_arg const arg) { regs->rdx = arg; }
void Kernel::Thread::user_arg_3(Kernel::Call_arg const arg) { regs->rcx = arg; }
void Kernel::Thread::user_arg_4(Kernel::Call_arg const arg) { regs->r8 = arg; }

Kernel::Call_arg Kernel::Thread::user_arg_0() const { return regs->rdi; }
Kernel::Call_arg Kernel::Thread::user_arg_1() const { return regs->rsi; }
Kernel::Call_arg Kernel::Thread::user_arg_2() const { return regs->rdx; }
Kernel::Call_arg Kernel::Thread::user_arg_3() const { return regs->rcx; }
Kernel::Call_arg Kernel::Thread::user_arg_4() const { return regs->r8; }
