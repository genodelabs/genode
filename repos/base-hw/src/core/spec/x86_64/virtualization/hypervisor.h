/*
 * \brief  Interface between kernel and hypervisor
 * \author Benjamin Lamowski
 * \date   2022-10-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SPEC__PC__VIRTUALIZATION_HYPERVISOR_H_
#define _SPEC__PC__VIRTUALIZATION_HYPERVISOR_H_

#include <base/stdint.h>
#include <base/log.h>

namespace Hypervisor {

	using Call_arg = Genode::umword_t;
	using Call_ret = Genode::umword_t;


	inline void restore_state_for_entry(Call_arg regs, Call_arg fpu_context)
	{
		asm volatile(
		      "fxrstor (%[fpu_context]);"
		      "mov  %[regs], %%rsp;"
		      "popq %%r8;"
		      "popq %%r9;"
		      "popq %%r10;"
		      "popq %%r11;"
		      "popq %%r12;"
		      "popq %%r13;"
		      "popq %%r14;"
		      "popq %%r15;"
		      "popq %%rax;"
		      "popq %%rbx;"
		      "popq %%rcx;"
		      "popq %%rdx;"
		      "popq %%rdi;"
		      "popq %%rsi;"
		      "popq %%rbp;"
		      "sti;" /* maybe enter the kernel to handle an external
		                interrupt that occured ... */
		      "nop;"
		      "cli;" /* ... otherwise, just disable interrupts again */
		      "jmp _kernel_entry;"
		      :
		      : [regs] "r"(regs), [fpu_context] "r"(fpu_context)

		      : "memory");
	};


	inline void switch_world(Call_arg guest_state, Call_arg regs,
	                         Call_arg fpu_context)
	{
		asm volatile(
		      "fxrstor (%[fpu_context]);"
		      "mov %[guest_state], %%rax;"
		      "mov  %[regs], %%rsp;"
		      "popq %%r8;"
		      "popq %%r9;"
		      "popq %%r10;"
		      "popq %%r11;"
		      "popq %%r12;"
		      "popq %%r13;"
		      "popq %%r14;"
		      "popq %%r15;"
		      "add $8, %%rsp;" /* don't pop rax */
		      "popq %%rbx;"
		      "popq %%rcx;"
		      "popq %%rdx;"
		      "popq %%rdi;"
		      "popq %%rsi;"
		      "popq %%rbp;"
		      "clgi;"
		      "sti;"
		      "vmload;"
		      "vmrun;"
		      "vmsave;"
		      "popq %%rax;" /* get the physical address of the host VMCB from
		                       the stack */
		      "vmload;"
		      "stgi;" /* maybe enter the kernel to handle an external interrupt
		                 that occured ... */
		      "nop;"
		      "cli;" /* ... otherwise, just disable interrupts again */
		      "pushq $256;" /* make the stack point to trapno, the right place
		                       to jump to _kernel_entry. We push 256 because
		                       this is outside of the valid range for interrupts
		                     */
		      "jmp _kernel_entry;" /* jump to _kernel_entry to save the
		                              GPRs without breaking any */
		      :
		      : [regs] "r"(regs), [fpu_context] "r"(fpu_context),
		      [guest_state] "r"(guest_state)
		      : "rax", "memory");
	}
}

#endif /* _SPEC__PC__VIRTUALIZATION_HYPERVISOR_H_ */
