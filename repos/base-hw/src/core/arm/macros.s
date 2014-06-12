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


/***************************************************
 ** Constant values that are pretty commonly used **
 ***************************************************/

/* alignment constraints */
.set min_page_size_log2, 12
.set data_access_alignm_log2, 2

