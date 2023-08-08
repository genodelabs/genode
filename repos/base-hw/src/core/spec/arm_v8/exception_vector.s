/*
 * \brief  Exception vector for ARMv8
 * \author Stefan Kalkowski
 * \date   2019-05-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.section .text.crt0

	.rept 16
	str  x0,  [sp, #-32]
	ldr  x0,  [sp, #-16]
	add  x0,  x0,  #8
	stp  x1,  x2,   [x0], #16
	stp  x3,  x4,   [x0], #16
	stp  x5,  x6,   [x0], #16
	stp  x7,  x8,   [x0], #16
	stp  x9,  x10,  [x0], #16
	stp  x11, x12,  [x0], #16
	stp  x13, x14,  [x0], #16
	stp  x15, x16,  [x0], #16
	stp  x17, x18,  [x0], #16
	stp  x19, x20,  [x0], #16
	stp  x21, x22,  [x0], #16
	stp  x23, x24,  [x0], #16
	stp  x25, x26,  [x0], #16
	stp  x27, x28,  [x0], #16
	stp  x29, x30,  [x0], #16
	mrs  x1, sp_el0
	mrs  x2, elr_el1
	mrs  x3, spsr_el1
	mrs  x4, mdscr_el1
	adr  x5, .
	and  x5, x5, #0xf80
	stp  x1, x2, [x0], #16
	stp  xzr, x3, [x0], #16   /* ec will be updated later if needed */
	stp  x4, x5, [x0], #16
	b _kernel_entry
	.balign 128
	.endr

_kernel_entry:
	stp  q0,  q1,  [x0], #32
	stp  q2,  q3,  [x0], #32
	stp  q4,  q5,  [x0], #32
	stp  q6,  q7,  [x0], #32
	stp  q8,  q9,  [x0], #32
	stp  q10, q11, [x0], #32
	stp  q12, q13, [x0], #32
	stp  q14, q15, [x0], #32
	stp  q16, q17, [x0], #32
	stp  q18, q19, [x0], #32
	stp  q20, q21, [x0], #32
	stp  q22, q23, [x0], #32
	stp  q24, q25, [x0], #32
	stp  q26, q27, [x0], #32
	stp  q28, q29, [x0], #32
	stp  q30, q31, [x0], #32
	mrs  x1,  fpcr
	mrs  x2,  fpsr
	stp  x1,  x2,  [x0], #16
	msr  fpsr, xzr
	ldr  x0,  [sp, #-16]
	ldr  x1,  [sp, #-32]
	str  x1,  [x0]
	bl _ZN6Kernel24main_handle_kernel_entryEv


.section .text

	/*******************************
	 ** idle loop for idle thread **
	 *******************************/

	.global idle_thread_main
	idle_thread_main:
	wfi
	b idle_thread_main


	/*****************************
	 ** kernel to userland switch **
	 *******************************/

	.global kernel_to_user_context_switch
	kernel_to_user_context_switch:
	mov  sp, x1               /* reset stack */
	str  x0, [sp, #-16]       /* store cpu state pointer */
	add  x1, x0, #8*31
	ldp  x2, x3, [x1], #16+8  /* load sp, ip, skip ec */
	ldp  x4, x5, [x1], #16+8  /* load pstate, mdscr_el1, skip exception_type */
	msr  sp_el0,   x2
	msr  elr_el1,  x3
	msr  spsr_el1, x4
	msr  mdscr_el1, x5
	ldp  q0,  q1,  [x1], #32
	ldp  q2,  q3,  [x1], #32
	ldp  q4,  q5,  [x1], #32
	ldp  q6,  q7,  [x1], #32
	ldp  q8,  q9,  [x1], #32
	ldp  q10, q11, [x1], #32
	ldp  q12, q13, [x1], #32
	ldp  q14, q15, [x1], #32
	ldp  q16, q17, [x1], #32
	ldp  q18, q19, [x1], #32
	ldp  q20, q21, [x1], #32
	ldp  q22, q23, [x1], #32
	ldp  q24, q25, [x1], #32
	ldp  q26, q27, [x1], #32
	ldp  q28, q29, [x1], #32
	ldp  q30, q31, [x1], #32
	ldp  x2,  x3,  [x1], #16
	msr  fpcr, x2
	msr  fpsr, x3
	add  x0,  x0,  #8
	ldp  x1,  x2,  [x0], #16
	ldp  x3,  x4,  [x0], #16
	ldp  x5,  x6,  [x0], #16
	ldp  x7,  x8,  [x0], #16
	ldp  x9,  x10, [x0], #16
	ldp  x11, x12, [x0], #16
	ldp  x13, x14, [x0], #16
	ldp  x15, x16, [x0], #16
	ldp  x17, x18, [x0], #16
	ldp  x19, x20, [x0], #16
	ldp  x21, x22, [x0], #16
	ldp  x23, x24, [x0], #16
	ldp  x25, x26, [x0], #16
	ldp  x27, x28, [x0], #16
	ldp  x29, x30, [x0]
	ldr  x0,  [x0, #-29*8]
	eret
