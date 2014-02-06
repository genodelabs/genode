/**
 * \brief   Startup code for Genode 64Bit applications
 * \author  Sebastian Sumpf
 * \author  Martin Stein
 * \date    2011-05-11
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/**************************
 ** .text (program code) **
 **************************/

.text

	/* program entry-point */
	.global _start
	_start:

	/* make initial value of some registers available to higher-level code */
	movq __initial_ax@GOTPCREL(%rip), %rbx
	movq %rax, (%rbx)
	movq __initial_di@GOTPCREL(%rip), %rbx
	movq %rdi, (%rbx)
	movq __initial_sp@GOTPCREL(%rip), %rax
	movq %rsp, (%rax)

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	leaq _stack_high@GOTPCREL(%rip),%rax
	movq (%rax), %rsp

	/* create proper environment for the main thread */
	call init_main_thread

	/* apply environment that was created by init_main_thread */
	movq init_main_thread_result@GOTPCREL(%rip), %rax
	movq (%rax), %rsp

	/* clear the base pointer in order that stack backtraces will work */
	xorq %rbp, %rbp

	/*
	 * We jump into initial C code instead of calling it as it should never
	 * return on the one hand and because the alignment of the stack pointer
	 * that init_main_thread returned expects a jump at the other hand. The
	 * latter matters because GCC expects the initial stack pointer to be
	 * aligned to 16 byte for at least the handling of floating points.
	 */
	jmp _main


/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.bss

	/* stack of the temporary initial environment */
	.p2align 8
	.global	_stack_low
	_stack_low:
	.space	64 * 1024
	.global	_stack_high
	_stack_high:

	/* initial value of the RSP, RAX and RDI register */
	.globl	__initial_sp
	__initial_sp:
	.space 8
	.globl	__initial_ax
	__initial_ax:
	.space 8
	.globl	__initial_di
	__initial_di:
	.space 8

	/* return value of init_main_thread */
	.globl init_main_thread_result
	init_main_thread_result:
	.space 8
