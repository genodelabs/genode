/*
 * \brief   Startup code for bootstrap
 * \author  Stefan Kalkowski
 * \date    2019-05-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.section ".text.crt0"

	.global _start
	_start:

	/***********************
	 ** Detect CPU number **
	 ***********************/

	mrs x0, mpidr_el1
	and x0, x0, #0b11111111
	cbz x0, _crt0_fill_bss_zero
	wfe
	b _start


	/***************************
	 ** Zero-fill BSS segment **
	 ***************************/

	_crt0_fill_bss_zero:
	ldr x0, =_bss_start
	ldr x1, =_bss_end
	1:
	cmp x1, x0
	b.eq _crt0_enable_fpu
	str xzr, [x0], #8
	b 1b


	/****************
	 ** Enable FPU **
	 ****************/

	_crt0_enable_fpu:
	mov x0, #0b11
	lsl x0, x0, #20
	msr cpacr_el1, x0


	/**********************
	 ** Initialize stack **
	 **********************/

	ldr x0, =_crt0_start_stack
	mov sp, x0
	bl init

	.p2align 4
	.space 0x4000
	_crt0_start_stack:
