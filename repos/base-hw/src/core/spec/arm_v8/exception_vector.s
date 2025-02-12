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
	sub  sp,  sp,  #0x340 /* keep room for cpu state */
	stp  x0,  x1,   [sp], #16
	stp  x2,  x3,   [sp], #16
	stp  x4,  x5,   [sp], #16
	stp  x6,  x7,   [sp], #16
	stp  x8,  x9,   [sp], #16
	stp  x10, x11,  [sp], #16
	stp  x12, x13,  [sp], #16
	stp  x14, x15,  [sp], #16
	stp  x16, x17,  [sp], #16
	stp  x18, x19,  [sp], #16
	stp  x20, x21,  [sp], #16
	stp  x22, x23,  [sp], #16
	stp  x24, x25,  [sp], #16
	stp  x26, x27,  [sp], #16
	stp  x28, x29,  [sp], #16
	mrs  x0, sp_el0
	mrs  x1, elr_el1
	mrs  x2, esr_el1
	mrs  x3, spsr_el1
	mrs  x4, mdscr_el1
	adr  x5, .
	and  x5, x5, #0xf80
	mrs  x6,  fpsr
	stp  x30, x0, [sp], #16
	stp  x1,  x2, [sp], #16
	stp  x3,  x4, [sp], #16
	stp  x5,  x6, [sp], #16
	b _kernel_entry
	.balign 128
	.endr

_kernel_entry:
	stp  q0,  q1,  [sp], #32
	stp  q2,  q3,  [sp], #32
	stp  q4,  q5,  [sp], #32
	stp  q6,  q7,  [sp], #32
	stp  q8,  q9,  [sp], #32
	stp  q10, q11, [sp], #32
	stp  q12, q13, [sp], #32
	stp  q14, q15, [sp], #32
	stp  q16, q17, [sp], #32
	stp  q18, q19, [sp], #32
	stp  q20, q21, [sp], #32
	stp  q22, q23, [sp], #32
	stp  q24, q25, [sp], #32
	stp  q26, q27, [sp], #32
	stp  q28, q29, [sp], #32
	stp  q30, q31, [sp], #32
	mrs  x0,  fpcr
	str  x0,  [sp], #8
	msr  fpsr, xzr
	sub  sp,  sp,  #0x338 /* overall cpu state size */
	mov  x0,  sp
	bl _ZN6Kernel24main_handle_kernel_entryEPN6Genode9Cpu_stateE


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
	add  x1, x0, #8*31
	ldp  x2, x3, [x1], #16+8  /* load sp, ip, skip esr_el1 */
	ldp  x4, x5, [x1], #16+8  /* load pstate, mdscr_el1, skip exception_type */
	msr  sp_el0,   x2
	msr  elr_el1,  x3
	msr  spsr_el1, x4
	msr  mdscr_el1, x5
	ldr  x2, [x1], #8
	msr  fpsr, x2
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
	ldr  x3,  [x1], #8
	msr  fpcr, x3
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
