/*
 * \brief   Macros that are used by multiple assembly files
 * \author  Martin Stein
 * \date    2014-01-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

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


/**
 * Calculate and apply kernel SP for a given kernel-stacks area
 *
 * \base_reg  register that contains the base of the kernel-stacks area
 * \size_reg  register that contains the size of one kernel stack
 */
.macro _init_kernel_sp base_reg, size_reg

	/* get kernel name of CPU */
	_get_cpu_id sp

	/* calculate top of the kernel-stack of this CPU and apply as SP */
	add sp, #1
	mul \size_reg, \size_reg, sp
	add sp, \base_reg, \size_reg
.endm


/**
 * Restore kernel SP from a given kernel context
 *
 * \context_reg  register that contains the base of the kernel context
 * \buf_reg_*    registers that can be used as local buffers
 */
.macro _restore_kernel_sp context_reg, buf_reg_0, buf_reg_1

	/* get base of the kernel-stacks area and the kernel-stack size */
	add sp, \context_reg, #R12_OFFSET
	ldm sp, {\buf_reg_0, \buf_reg_1}

	/* calculate and apply kernel SP */
	_init_kernel_sp \buf_reg_1, \buf_reg_0
.endm


/***************************************************
 ** Constant values that are pretty commonly used **
 ***************************************************/

/* alignment constraints */
.set MIN_PAGE_SIZE_LOG2, 12
.set DATA_ACCESS_ALIGNM_LOG2, 2


/***************************************************
 ** Constant values that the mode transition uses **
 ***************************************************/

/* kernel names of exceptions that can interrupt a user */
.set RST_TYPE, 1
.set UND_TYPE, 2
.set SVC_TYPE, 3
.set PAB_TYPE, 4
.set DAB_TYPE, 5
.set IRQ_TYPE, 6
.set FIQ_TYPE, 7

.set RST_PC_ADJUST, 0
.set UND_PC_ADJUST, 4
.set SVC_PC_ADJUST, 0
.set PAB_PC_ADJUST, 4
.set DAB_PC_ADJUST, 8
.set IRQ_PC_ADJUST, 4
.set FIQ_PC_ADJUST, 4

/* offsets of the member variables in a CPU context */
.set R12_OFFSET,            12 * 4
.set SP_OFFSET,             13 * 4
.set LR_OFFSET,             14 * 4
.set PC_OFFSET,             15 * 4
.set PSR_OFFSET,            16 * 4
.set EXCEPTION_TYPE_OFFSET, 17 * 4
.set TRANSIT_TTBR0_OFFSET,  17 * 4
.set CIDR_OFFSET,           18 * 4
.set TTBR0_OFFSET,          19 * 4
.set TTBCR_OFFSET,          20 * 4
.set MAIR0_OFFSET,          21 * 4

/* size of local variables */
.set CONTEXT_PTR_SIZE, 1 * 4


/*********************************************************
 ** Local data structures that the mode transition uses **
 *********************************************************/

.macro _mt_local_variables

	/* space for a copy of the kernel context */
	.p2align 2
	.global _mt_master_context_begin
	_mt_master_context_begin:
	.space 32 * 4
	.global _mt_master_context_end
	_mt_master_context_end:

	/* space for a client context-pointer per CPU */
	.p2align 2
	.global _mt_client_context_ptr
	_mt_client_context_ptr:
	.rept NR_OF_CPUS
		.space CONTEXT_PTR_SIZE
	.endr

	/* a globally mapped buffer per CPU */
	.p2align 2
	.global _mt_buffer
	_mt_buffer:
	.rept NR_OF_CPUS
		.space BUFFER_SIZE
	.endr

.endm /* _mt_local_variables */
