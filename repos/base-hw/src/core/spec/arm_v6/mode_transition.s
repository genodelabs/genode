/*
 * \brief  Transition between kernel and userland
 * \author Martin stein
 * \date   2011-11-15
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
.include "macros.s"


/***************
 ** Constants **
 ***************/

/* size of local variables */
.set BUFFER_SIZE, 1 * 4


/************
 ** Macros **
 ************/

/**
 * Invalidate all entries of the branch prediction cache
 *
 * FIXME branch prediction shall not be activated for now because we have no
 *       support for instruction barriers. The manual says that one should
 *       implement this via 'swi 0xf00000', but when we do this in SVC mode it
 *       pollutes our SP and this is not acceptable with the current mode
 *       transition implementation
 */
.macro _flush_branch_predictor
	mcr p15, 0, sp, c7, c5, 6
	/*swi 0xf00000 */
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

	/* load kernel cidr */
	adr sp, _mt_master_context_begin
	ldr sp, [sp, #CIDR_OFFSET]
	mcr p15, 0, sp, c13, c0, 1
	_flush_branch_predictor

	/* load kernel ttbr0 */
	adr sp, _mt_master_context_begin
	ldr sp, [sp, #TTBR0_OFFSET]
	mcr p15, 0, sp, c2, c0, 0
	_flush_branch_predictor

	/*******************************************
	 ** Now it's save to access kernel memory **
	 *******************************************/

	/* get user context pointer */
	ldr sp, _mt_client_context_ptr

	/*
	 * Save user r0 ... r12. We explicitely target user registers
	 * via '^' because we might be in FIQ exception-mode where
	 * some of them are banked. Doesn't affect other modes.
	 */
	stmia sp, {r0-r12}^

	/* save user lr and sp */
	add r0, sp, #SP_OFFSET
	stmia r0, {sp,lr}^

	/* adjust and save user pc */
	.if \pc_adjust != 0
		sub lr, lr, #\pc_adjust
	.endif
	str lr, [sp, #PC_OFFSET]

	/* save user psr */
	mrs r0, spsr
	str r0, [sp, #PSR_OFFSET]

	/* save type of exception that interrupted the user */
	mov r0, #\exception_type
	str r0, [sp, #EXCEPTION_TYPE_OFFSET]

	/*
	 * Switch to supervisor mode
	 *
	 * FIXME This is done due to incorrect behavior when running the kernel
	 *       high-level-code in FIQ-exception mode. Please debug this behavior
	 *       and remove this switch.
	 */
	cps #19

	/* apply kernel sp */
	adr r0, _mt_master_context_begin
	_restore_kernel_sp r0, r1, r2

	/* load kernel context */
	add r0, r0, #LR_OFFSET
	ldm r0, {lr, pc}

.endm


/**********************************
 ** Linked into the text section **
 **********************************/

.section .text

	/*
	 * Page aligned base of mode transition code.
	 *
	 * This position independent code switches between a kernel context and a
	 * user context and thereby between their address spaces. Due to the latter
	 * it must be mapped executable to the same region in every address space.
	 * To enable such switching, the kernel context must be stored within this
	 * region, thus one should map it solely accessable for privileged modes.
	 */
	.p2align MIN_PAGE_SIZE_LOG2
	.global _mt_begin
	_mt_begin:

	/*
	 * On user exceptions the CPU has to jump to one of the following
	 * seven entry vectors to switch to a kernel context.
	 */
	.global _mt_kernel_entry_pic
	_mt_kernel_entry_pic:

	b _rst_entry /* 0x00: reset                  */
	b _und_entry /* 0x04: undefined instruction  */
	b _swi_entry /* 0x08: software interrupt     */
	b _pab_entry /* 0x0c: prefetch abort         */
	b _dab_entry /* 0x10: data abort             */
	nop          /* 0x14: reserved               */
	b _irq_entry /* 0x18: interrupt request      */
	b _fiq_entry /* 0x1c: fast interrupt request */

	/* PICs that switch from a user exception to the kernel */
	_rst_entry: _user_to_kernel_pic RST_TYPE, RST_PC_ADJUST
	_und_entry: _user_to_kernel_pic UND_TYPE, UND_PC_ADJUST
	_swi_entry:

	/*
	 * FIXME fast SWI routines pollute the SVC SP but we have
	 *       to call them especially in SVC mode
	 */

	/* check if SWI requests a fast service routine */
	/*ldr sp, [r14, #-0x4]*/
	/*and sp, sp, #0xffffff*/

	/* fast "instruction barrier" service routine */
	/*cmp sp, #0xf00000*/
	/*bne _slow_swi_entry*/
	/*movs pc, r14*/

	/* slow high level service routine */
	_slow_swi_entry: _user_to_kernel_pic SVC_TYPE, SVC_PC_ADJUST

	_pab_entry: _user_to_kernel_pic PAB_TYPE, PAB_PC_ADJUST
	_dab_entry: _user_to_kernel_pic DAB_TYPE, DAB_PC_ADJUST
	_irq_entry: _user_to_kernel_pic IRQ_TYPE, IRQ_PC_ADJUST
	_fiq_entry: _user_to_kernel_pic FIQ_TYPE, FIQ_PC_ADJUST

	/* kernel must jump to this point to switch to a user context */
	.p2align 2
	.global _mt_user_entry_pic
	_mt_user_entry_pic:

	/* get user context pointer */
	ldr lr, _mt_client_context_ptr

	/* buffer user pc */
	ldr r0, [lr, #PC_OFFSET]
	adr r1, _mt_buffer
	str r0, [r1]

	/* buffer user psr */
	ldr r0, [lr, #PSR_OFFSET]
	msr spsr, r0

	/* load user r0 ... r12 */
	ldm lr, {r0-r12}

	/* load user sp and lr */
	add sp, lr, #SP_OFFSET
	ldm sp, {sp,lr}^

	/* get user cidr and ttbr0 */
	ldr sp, [lr, #CIDR_OFFSET]
	ldr lr, [lr, #TTBR0_OFFSET]

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
	ldm lr, {pc}^

	_mt_local_variables

	.p2align 2
	.global _mt_end
	_mt_end:

	/* FIXME exists only because _vm_mon_entry pollutes generic kernel code */
	.global _mt_vm_entry_pic
	_mt_vm_entry_pic:
	1: b 1b
