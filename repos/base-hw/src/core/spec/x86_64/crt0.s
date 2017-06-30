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

.section ".text"

	/***********************
	 ** kernel entry code **
	 ***********************/

	.global _start
	_start:

	/* switch to kernel stack */
	mov kernel_stack@GOTPCREL(%rip), %rax
	mov kernel_stack_size@GOTPCREL(%rip), %rbx
	add (%rbx), %rax
	mov  %rax,  %rsp

	/* jump to C entry code */
	jmp kernel_init


	/*********************************
	 ** core main thread entry code **
	 *********************************/

	.global _core_start
	_core_start:

	/* initialize GLOBAL OFFSET TABLE */
	leaq _GLOBAL_OFFSET_TABLE_(%rip), %r15

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
