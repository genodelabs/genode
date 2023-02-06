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

/**
 * Store CPU number in register x0
 */
.macro _cpu_number
	mrs x0, mpidr_el1
	and x8, x0, #(1<<24) /* MT bit */
	cbz x8, 1f
	lsr x0, x0, #8
1:
	and x0, x0, #0b11111111
.endm

.section ".text.crt0"

	.global _start
	_start:

	bl _mmu_disable

	/**
	 * Hack for Qemu, which starts all cpus at once
	 * only first CPU runs through, all others wait for wakeup
	 */
	_cpu_number
	cbz x0, _crt0_fill_bss_zero
	1:
	ldr  x1, =_crt0_qemu_start_secondary_cpus
	ldr  w1, [x1]
	cbnz w1, _crt0_enable_fpu
	wfe
	b 1b
	.global _crt0_qemu_start_secondary_cpus
	_crt0_qemu_start_secondary_cpus:
	.long 0


	/*************************************
	 ** Disable MMU of current EL (1-3) **
	 *************************************/

	_mmu_disable:
	mrs  x8, CurrentEL
	lsr  x8, x8, #2
	cmp  x8, #0x2
	b.eq _el2
	b.hi _el3
	_el1:
	mrs x8, sctlr_el1
	bic x8, x8, #(1 << 0)
	msr sctlr_el1, x8
	isb
	ret
	_el2:
	mrs x8, sctlr_el2
	bic x8, x8, #(1 << 0)
	msr sctlr_el2, x8
	isb
	ret
	_el3:
	mrs x8, sctlr_el3
	bic x8, x8, #(1 << 0)
	msr sctlr_el3, x8
	isb
	ret


	/***************************
	 ** Zero-fill BSS segment **
	 ***************************/

	_crt0_fill_bss_zero:
	ldr x1, =_bss_start
	ldr x2, =_bss_end
	1:
	cmp x2, x1
	b.eq _crt0_enable_fpu
	str xzr, [x1], #8
	b 1b

	/************************************
	 ** Common Entrypoint for all CPUs **
	 ************************************/

	.global _crt0_start_secondary
	_crt0_start_secondary:

	bl _mmu_disable


	/****************
	 ** Enable FPU **
	 ****************/

	_crt0_enable_fpu:
	mov x1, #0b11
	lsl x1, x1, #20
	msr cpacr_el1, x1


	/**********************
	 ** Initialize stack **
	 **********************/

	_cpu_number
	ldr x1, =bootstrap_stack
	ldr x2, =bootstrap_stack_size
	ldr x2, [x2]
	add x0, x0, #1
	mul x0, x0, x2
	add x1, x1, x0
	mov sp, x1
	bl init
