/*
 * \brief  Transition between kernel and userland
 * \author Martin stein
 * \date   2011-11-15
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/**
 * Invalidate all entries of the branch predictor array
 */
.macro _flush_branch_predictor
	mcr p15, 0, sp, c7, c5, 6
	isb
.endm

/**
 * Switch from an interrupted user context to a kernel context
 *
 * \param exception_type  immediate exception type ID
 * \param pc_adjust       immediate value that gets subtracted from the
 *                        user PC before it gets saved
 */
.macro _user_to_kernel_pic exception_type, pc_adjust

	/*
	 * We expect that privileged modes are never interrupted by an
	 * exception. Thus we can assume that we always come from
	 * user mode at this point.
	 */

	/************************************************
	 ** We're still in the user protection domain, **
	 ** so we must avoid access to kernel memory   **
	 ************************************************/

	/* load kernel contextidr */
	adr sp, _mt_kernel_context_begin
	ldr sp, [sp, #17*4]
	mcr p15, 0, sp, c13, c0, 1
	_flush_branch_predictor

	/* load kernel section table */
	adr sp, _mt_kernel_context_begin
	ldr sp, [sp, #19*4]
	mcr p15, 0, sp, c2, c0, 0
	_flush_branch_predictor

	/*******************************************
	 ** Now it's save to access kernel memory **
	 *******************************************/

	/* get user context pointer */
	ldr sp, _mt_user_context_ptr

	/*
	 * Save user r0 ... r12. We explicitely target user registers
	 * via '^' because we might be in FIQ exception-mode where
	 * some of them are banked. Doesn't affect other modes.
	 */
	stmia sp, {r0-r12}^

	/* save user lr and sp */
	add r0, sp, #13*4
	stmia r0, {sp,lr}^

	/* adjust and save user pc */
	.if \pc_adjust != 0
		sub lr, lr, #\pc_adjust
	.endif
	str lr, [sp, #15*4]

	/* save user psr */
	mrs r0, spsr
	str r0, [sp, #16*4]

	/* save type of exception that interrupted the user */
	mov r0, #\exception_type
	str r0, [sp, #18*4]

	/*
	 * Switch to supervisor mode
	 * FIXME This is done due to incorrect behavior when running the kernel
	 *       high-level-code in FIQ-exception mode. Please debug this behavior
	 *       and remove this switch.
	 */
	cps #19

	/* get kernel context pointer */
	adr r0, _mt_kernel_context_begin

	/* load kernel context */
	add r0, r0, #13*4
	ldmia r0, {sp, lr, pc}

.endm


.section .text

	/*
	 * The mode transition PIC switches between a kernel context and a user
	 * context and thereby between their address spaces. Due to the latter
	 * it must be mapped executable to the same region in every address space.
	 * To enable such switching, the kernel context must be stored within this
	 * region, thus one should map it solely accessable for privileged modes.
	 */
	.align 3
	.global _mode_transition_begin
	_mode_transition_begin:

		/*
		 * On user exceptions the CPU has to jump to one of the following
		 * 7 entry vectors to switch to a kernel context.
		 */
		.align 3
		.global _mt_kernel_entry_pic
		_mt_kernel_entry_pic:

			b _rst_entry  /* reset                  */
			b _und_entry  /* undefined instruction  */
			b _svc_entry  /* supervisor call        */
			b _pab_entry  /* prefetch abort         */
			b _dab_entry  /* data abort             */
			nop           /* reserved               */
			b _irq_entry  /* interrupt request      */
			b _fiq_entry  /* fast interrupt request */

			/* PICs that switch from an user exception to the kernel */
			_rst_entry: _user_to_kernel_pic 1, 0
			_und_entry: _user_to_kernel_pic 2, 4
			_svc_entry: _user_to_kernel_pic 3, 0
			_pab_entry: _user_to_kernel_pic 4, 4
			_dab_entry: _user_to_kernel_pic 5, 8
			_irq_entry: _user_to_kernel_pic 6, 4
			_fiq_entry: _user_to_kernel_pic 7, 4

		/* kernel must jump to this point to switch to a user context */
		.align 3
		.global _mt_user_entry_pic
		_mt_user_entry_pic:

			/* get user context pointer */
			ldr lr, _mt_user_context_ptr

			/* buffer user pc */
			ldr r0, [lr, #15*4]
			adr r1, _mt_buffer
			str r0, [r1]

			/* buffer user psr */
			ldr r0, [lr, #16*4]
			msr spsr, r0

			/* load user r0 ... r12 */
			ldmia lr, {r0-r12}

			/* load user sp and lr */
			add sp, lr, #13*4
			ldmia sp, {sp,lr}^

			/* get user contextidr and section table */
			ldr sp, [lr, #17*4]
			ldr lr, [lr, #19*4]

			/********************************************************
			 ** From now on, until we leave kernel mode, we must   **
			 ** avoid access to memory that is not mapped globally **
			 ********************************************************/

			/* apply user contextidr and section table */
			mcr p15, 0, sp, c13, c0, 1
			mcr p15, 0, lr, c2, c0, 0
			_flush_branch_predictor

			/* load user pc (implies application of the user psr) */
			adr lr, _mt_buffer
			ldmia lr, {pc}^

		/* leave some space for the kernel context */
		.align 3
		.global _mt_kernel_context_begin
		_mt_kernel_context_begin: .space 32*4
		.global _mt_kernel_context_end
		_mt_kernel_context_end:

		/* pointer to the user context backup space */
		.align 3
		.global _mt_user_context_ptr
		_mt_user_context_ptr: .long 0

		/* a local word-sized buffer */
		.align 3
		.global _mt_buffer
		_mt_buffer: .long 0

	.align 3
	.global _mode_transition_end
	_mode_transition_end:

