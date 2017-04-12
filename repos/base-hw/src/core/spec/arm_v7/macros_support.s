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

/* core includes */
.include "spec/arm/macros_support.s"

/**
 * Load kernel name of the executing CPU into register 'r'
 */
.macro _get_cpu_id r

	/* read the multiprocessor affinity register */
	mrc p15, 0, \r, c0, c0, 5

	/* get the affinity-0 bitfield from the read register value */
	and \r, \r, #0xff
.endm


/**
 * Determine the base of the client context of the executing CPU
 *
 * \param target_reg          register that shall receive the base pointer
 * \param buf_reg             register that can be polluted by the macro
 * \param client_context_ptr  label of the client context pointer base
 */
.macro _get_client_context_ptr target_reg, buf_reg, client_context_ptr

	/* get kernel name of CPU */
	_get_cpu_id \buf_reg

	/* multiply CPU name with pointer size to get offset of pointer */
	mov \target_reg, #CONTEXT_PTR_SIZE
	mul \buf_reg, \buf_reg, \target_reg

	/* get base of the pointer array */
	adr \target_reg, \client_context_ptr

	/* add offset and base to get CPU-local pointer */
	add \target_reg, \target_reg, \buf_reg
	ldr \target_reg, [\target_reg]
.endm


/**
 * Save sp, lr and spsr register banks of specified exception mode
 */
.macro _save_bank mode
	cps   #\mode          /* switch to given mode                  */
	mrs   r1, spsr        /* store mode-specific spsr              */
	stmia r0!, {r1,sp,lr} /* store mode-specific sp and lr         */
.endm /* _save_bank mode */


/**
 * Restore sp, lr and spsr register banks of specified exception mode
 */
.macro _restore_bank mode
	cps   #\mode          /* switch to given mode                        */
	ldmia r0!, {r1,sp,lr} /* load mode-specific sp, lr, and spsr into r1 */
	msr   spsr_cxfs, r1   /* load mode-specific spsr                     */
.endm


/***************
 ** Constants **
 ***************/

/* hardware names of CPU modes */
.set USR_MODE, 16
.set FIQ_MODE, 17
.set IRQ_MODE, 18
.set SVC_MODE, 19
.set ABT_MODE, 23
.set UND_MODE, 27

