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

/**
 * Calculate and apply kernel SP for a given kernel-stacks area
 *
 * \base_reg  register that contains the base of the kernel-stacks area
 * \size_reg  register that contains the size of one kernel stack
 */
.macro _init_kernel_sp base_reg, size_reg

	/* get kernel name of processor */
	_get_processor_id sp

	/* calculate top of the kernel-stack of this processor and apply as SP */
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
