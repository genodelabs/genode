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

.section ".text.crt0"

	/* program entry-point */
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

	/* prepare the first call of the kernel main-routine */
	ldr sp, =_kernel_stack_high
	bl setup_kernel

	/* call the kernel main-routine */
	ldr sp, =_kernel_stack_high
	bl kernel

	/* catch erroneous return of the kernel main-routine */
	3: b 3b

.section .bss

	/* kernel stack, must be aligned to an 8-byte boundary */
	.align 3
	.space 64 * 1024
	.global _kernel_stack_high
	_kernel_stack_high:

