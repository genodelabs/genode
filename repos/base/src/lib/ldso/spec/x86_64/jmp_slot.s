/**
 * \brief  Jump slot entry code for x86_64
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.text
.align 4
.globl _jmp_slot
.type _jmp_slot,@function
_jmp_slot:
	subq $8, %rsp
	pushfq
	pushq %rax
	pushq %rdx
	pushq %rcx
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11

	/* obj pointer */
	movq 0x58(%rsp), %rdi
	/* relocation index */
	movq 0x60(%rsp), %rsi

	call jmp_slot@PLT
	/* rax now contains target symbol address */
	movq %rax, 0x60(%rsp)

	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rdi
	popq %rsi
	popq %rcx
	popq %rdx
	popq %rax
	popfq
	leaq 16(%rsp), %rsp

	ret
