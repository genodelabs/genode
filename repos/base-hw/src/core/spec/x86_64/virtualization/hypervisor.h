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
}

#endif /* _SPEC__PC__VIRTUALIZATION_HYPERVISOR_H_ */
