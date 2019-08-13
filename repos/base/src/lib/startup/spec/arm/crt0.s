/**
 * \brief   Startup code for Genode applications on ARM
 * \author  Norman Feske
 * \author  Martin Stein
 * \date    2007-04-28
 */

/*
 * Copyright (C) 2007-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/**************************
 ** .text (program code) **
 **************************/

/**
 * Set pointer to global offset table in given register
 */
.macro _got reg
1:
	ldr \reg, 2f
	add \reg, pc, \reg
	b 3f
2:
	.word _GLOBAL_OFFSET_TABLE_ - (1b + 12)
3:
.endm

.section ".text.crt0"

	/* ld.lib.so entry for Linux */
	.global _start_initial_stack
	_start_initial_stack:

	/* on Linux we have a valid stack pointer */
	bl init_rtld

	/* save stack pointer in __intial_sp  GOT relative */
	_got sl
	ldr r4, .LGOT
	ldr r4, [sl, r4]
	str sp, [r4]

	/* load stack pointer _stack_high GOT relative */
	ldr r4, .LGOT + 8
	ldr sp, [sl, r4]

	b 4f

	/* program entry-point */
	.global _start
	_start:

	_got sl

	/* save __initial_sp */
	ldr r4, .LGOT
	ldr r4, [sl, r4]
	str sp, [r4]

	/* r0 contains the boot info on SeL4 save in __intial_r0 */
	ldr r4, .LGOT + 4
	ldr r4, [sl, r4]
	str r0, [r4]

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates, load __stack_high
	 */
	ldr r4, .LGOT + 8
	ldr sp, [sl, r4]

	/* if this is the dynamic linker, init_rtld relocates the linker */
	bl init_rtld

4:

	/* create proper environment for main thread */
	bl init_main_thread

	/*
	 * Apply environment that was created by init_main_thread from
	 * init_main_thread_result
	 */
	ldr r4, .LGOT + 12
	ldr sp, [sl, r4]
	ldr sp, [sp]

	/* jump into init C code instead of calling it as it should never return */
	b _main

.LGOT:
	.word __initial_sp(GOT)
	.word __initial_r0(GOT)
	.word _stack_high(GOT)
	.word init_main_thread_result(GOT)


/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.section ".bss"

	/* stack of the temporary initial environment */
	.p2align 4
	.global __initial_stack_base
	__initial_stack_base:
	.space 4*1024
	.global _stack_high
	_stack_high:

	/* initial value of the SP register */
	.global __initial_sp
	__initial_sp:
	.space 4
	/* initial value of the R0 register */
	.global __initial_r0
	__initial_r0:
	.space 4
