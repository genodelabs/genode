/**
 * \brief   Startup code for Genode programs on Cortex A9
 * \author  Martin Stein
 * \date    2011-10-01
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.section .text

	/* ELF entry symbol */
	.global _start
	_start:

		/* zero-fill BSS segment but don't pollute thread-entry arguments */
		.extern _bss_start
		ldr r4, =_bss_start
		.extern _bss_end
		ldr r3, =_bss_end
		mov r2, #0
		sub r3, r3, #4
		1:
		str r2, [r4]
		add r4, r4, #4
		cmp r4, r3
		bne 1b

		/* fetch thread-entry arguments to their destinations in BSS */
		ldr r1, =_main_utcb
		str r0, [r1]

		/* call _main routine */
		ldr sp, =_main_stack_high
		.extern _main
		bl _main
		1:
		b 1b

	/* dynamic symbol object handle */
	.align 3
	.global __dso_handle
	__dso_handle: .long 0

.section .bss

	/* main-thread stack */
	.align 3
	.space 64*1024
	.global _main_stack_high
	_main_stack_high:

	/* main-thread UTCB-pointer for the Genode thread-API */
	.align 3
	.global _main_utcb
	_main_utcb: .long 0

