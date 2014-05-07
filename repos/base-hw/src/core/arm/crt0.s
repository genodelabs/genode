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

.include "macros.s"


/**
 * Get base of the first kernel-stack and the common kernel-stack size
 *
 * \param base_dst_reg  register that shall receive the stack-area base
 * \param size_dst_reg  register that shall receive the size of a kernel stack
 */
.macro _get_constraints_of_kernel_stacks base_dst_reg, size_dst_reg

	ldr \base_dst_reg, =kernel_stack
	ldr \size_dst_reg, =kernel_stack_size
	ldr \size_dst_reg, [\size_dst_reg]
.endm


.section ".text.crt0"

	/****************************************
	 ** Startup code for primary processor **
	 ****************************************/

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

	/* setup temporary stack pointer for uniprocessor mode */
	_get_constraints_of_kernel_stacks r0, r1
	add sp, r0, r1

	/* uniprocessor kernel-initialization which activates multiprocessor */
	bl init_kernel_uniprocessor

	/***************************************************
	 ** Startup code that is common to all processors **
	 ***************************************************/

	.global _start_secondary_processors
	_start_secondary_processors:

	/* setup multiprocessor-aware kernel stack-pointer */
	_get_constraints_of_kernel_stacks r0, r1
	_init_kernel_sp r0, r1

	/* do multiprocessor kernel-initialization */
	bl init_kernel_multiprocessor

	/* call the kernel main-routine */
	bl kernel

	/* catch erroneous return of the kernel main-routine */
	1: b 1b
