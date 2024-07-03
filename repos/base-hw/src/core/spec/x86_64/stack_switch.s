/*
 * \brief  Assembler macros for x86 kernel stack switch
 * \author Stefan Kalkowski
 * \date   2024-07-02
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.include "memory_consts.s"

/* calculate stack from cpu memory area and cpu apic id */
.macro switch_to_kernel_stack
	/* request cpuid 1, result in ebx contains 1-byte apic id at bit 24 */
	movq $1, %rax
	cpuid
	shr  $24, %rbx
	and  $0xff, %rbx

	/* calculate cpu local memory slot by apic id */
	movq $HW_MM_CPU_LOCAL_MEMORY_SLOT_SIZE, %rax
	mulq %rbx
	movq $HW_MM_CPU_LOCAL_MEMORY_AREA_START, %rbx
	addq %rbx, %rax

	/* calculate top of stack and switch to it */
	movq $HW_MM_KERNEL_STACK_SIZE, %rbx
	addq %rbx, %rax
	subq $8, %rax
	movq %rax, %rsp
.endm
