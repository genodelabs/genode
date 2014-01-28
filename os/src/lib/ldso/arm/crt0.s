/**
 * \brief   Startup code for Genode applications on ARM
 * \author  Norman Feske
 * \author  Martin Stein
 * \date    2007-04-28
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/**************************
 ** .text (program code) **
 **************************/

.section ".text.crt0"

	/* linker entry-point */
	.globl _start_ldso
	_start_ldso:

	/* make initial value of some registers available to higher-level code */
	ldr r2, =__initial_sp
	str sp, [r2]

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	ldr  sp, =_stack_high

	/* let init_rtld relocate linker */
	bl init_rtld

	/* create proper environment for the main thread */
	bl init_main_thread

	/* apply environment that was created by init_main_thread */
	ldr sp, =init_main_thread_result
	ldr sp, [sp]

	/* call init C code */
	bl _main

	/* this should never be reached since _main should never return */
	_catch_main_return:
	b _catch_main_return
