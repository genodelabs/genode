/**
 * \brief   Startup code for Genode applications on ARM 64-bit
 * \author  Stefan Kalkowski
 * \date    2019-03-26
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/**************************
 ** .text (program code) **
 **************************/

.section ".text.crt0"

	/* program entry-point */
	.global _start
	_start:
	.global _start_initial_stack
	_start_initial_stack:

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	adrp x4, :got:__initial_stack_high
	ldr  x4, [x4, #:got_lo12:__initial_stack_high]
	mov  sp, x4

	/* if this is the dynamic linker, init_rtld relocates the linker */
	bl init_rtld

	/* create proper environment for main thread */
	bl init_main_thread

	/* apply environment that was created by init_main_thread */
	adrp x4, :got:init_main_thread_result
	ldr  x4, [x4, #:got_lo12:init_main_thread_result]
	ldr  x4, [x4]
	mov sp, x4

	/* jump into init C code instead of calling it as it should never return */
	b _main


/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.bss

	/* stack of the temporary initial environment */
	.p2align 8
	.global __initial_stack_base
	__initial_stack_base:
	.space 12*1024
	.global __initial_stack_high
	__initial_stack_high:
