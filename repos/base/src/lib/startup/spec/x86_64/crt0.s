/**
 * \brief   Startup code for Genode 64Bit applications
 * \author  Sebastian Sumpf
 * \author  Martin Stein
 * \date    2011-05-11
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/**************************
 ** .text (program code) **
 **************************/

.section ".text.crt0"

	/* ld.lib.so entry point for Linux */
	.global _start_initial_stack
	_start_initial_stack:

	/* initialize GLOBAL OFFSET TABLE */
	leaq _GLOBAL_OFFSET_TABLE_(%rip), %r15

	/* init_rtld relocates the linker */
	call init_rtld

	/* the address of __initial_sp is now correct */
	movq __initial_sp@GOTPCREL(%rip), %rax
	movq %rsp, (%rax)

	jmp 1f

	/* program entry-point */
	.global _start
	_start:

	/* initialize GLOBAL OFFSET TABLE */
	leaq _GLOBAL_OFFSET_TABLE_(%rip), %r15

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

	/* init_rtld relocates the linker */
	call init_rtld

1:
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
	.global __initial_stack_base
	__initial_stack_base:
	.space 12*1024
	_stack_high:

	/* initial value of the RSP, RAX and RDI register */
	.global __initial_sp
	__initial_sp:
	.space 8
	.global __initial_ax
	__initial_ax:
	.space 8
	.global __initial_di
	__initial_di:
	.space 8
