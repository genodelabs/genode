/*
 * \brief   Macros that are used by multiple assembly files
 * \author  Martin Stein
 * \date    2014-01-13
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.include "macros_arm.s"


/*******************
 ** Common macros **
 *******************/

/**
 * Determine the top of the kernel stack of this processor and apply it as SP
 *
 * \base_reg  register that contains the base of the kernel-stacks area
 * \size_reg  register that contains the size of one kernel stack
 */
.macro _init_kernel_sp base_reg, size_reg, buf_reg

	/* get kernel name of processor */
	_get_processor_id sp

	/* calculate top of the kernel-stack of this processor and apply as SP */
	add sp, #1
	mul \size_reg, \size_reg, sp
	add sp, \base_reg, \size_reg
.endm


/******************************************
 ** Macros regarding the mode transition **
 ******************************************/

/**
 * Constant values that the mode transition uses
 */
.macro _mt_constants

	/* kernel names of exceptions that can interrupt a user */
	.set rst_type, 1
	.set und_type, 2
	.set svc_type, 3
	.set pab_type, 4
	.set dab_type, 5
	.set irq_type, 6
	.set fiq_type, 7

	.set rst_pc_adjust, 0
	.set und_pc_adjust, 4
	.set svc_pc_adjust, 0
	.set pab_pc_adjust, 4
	.set dab_pc_adjust, 8
	.set irq_pc_adjust, 4
	.set fiq_pc_adjust, 4

	/* offsets of the member variables in a processor context */
	.set r12_offset,            12 * 4
	.set sp_offset,             13 * 4
	.set lr_offset,             14 * 4
	.set pc_offset,             15 * 4
	.set psr_offset,            16 * 4
	.set exception_type_offset, 17 * 4
	.set contextidr_offset,     18 * 4
	.set section_table_offset,  19 * 4

	/* alignment constraints */
	.set min_page_size_log2, 12

	/* size of local variables */
	.set context_ptr_size, 1 * 4
.endm

/**
 * Local data structures that the mode transition uses
 */
.macro _mt_local_variables

	/* space for a copy of the kernel context */
	.p2align 2
	.global _mt_master_context_begin
	_mt_master_context_begin:
	.space 32 * 4
	.global _mt_master_context_end
	_mt_master_context_end:

	/* space for a client context-pointer per processor */
	.p2align 2
	.global _mt_client_context_ptr
	_mt_client_context_ptr:
	.rept PROCESSORS
		.space context_ptr_size
	.endr

	/* a globally mapped buffer per processor */
	.p2align 2
	.global _mt_buffer
	_mt_buffer:
	.rept PROCESSORS
		.space buffer_size
	.endr
.endm
