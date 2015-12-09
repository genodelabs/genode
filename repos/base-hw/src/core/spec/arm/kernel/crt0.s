/*
 * \brief   Startup code for core
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2011-10-01
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/************
 ** Macros **
 ************/

/* core includes */
.include "macros.s"


.section ".text.crt0"

	/**********************************
	 ** Startup code for primary CPU **
	 **********************************/

	.global _start
	_start:

	/* idle a little initially because U-Boot likes it this way */
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8
	mov r8, r8

	/* zero-fill BSS segment */
	ldr r0, =_bss_start
	ldr r1, =_bss_end
	mov r2, #0
	1:
	cmp r1, r0
	ble 2f
	str r2, [r0]
	add r0, r0, #4
	b 1b
	2:

	/* setup temporary stack pointer */
	_get_constraints_of_kernel_stacks r0, r1
	add sp, r0, r1

	/* kernel-initialization */
	bl init_kernel

	/* catch erroneous return of the kernel initialization routine */
	1: b 1b
