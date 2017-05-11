/*
 * \brief  Transition between kernel/userland, and secure/non-secure world
 * \author Martin Stein
 * \author Stefan Kalkowski
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

/* size of local variables */
.set BUFFER_SIZE, 3 * 4


/************
 ** Macros **
 ************/

/**
 * Determine the base of the globally mapped buffer of the executing CPU
 *
 * \param target_reg  register that shall receive the base pointer
 * \param buf_reg     register that can be polluted by the macro
 */
.macro _get_buffer_ptr target_reg, buf_reg

	/* get kernel name of CPU */
	_get_cpu_id \buf_reg

	/* multiply CPU name with buffer size to get offset of buffer */
	mov \target_reg, #BUFFER_SIZE
	mul \buf_reg, \buf_reg, \target_reg

	/* get base of the buffer array */
	adr \target_reg, _mt_buffer

	/* add offset and base to get CPU-local buffer */
	add \target_reg, \target_reg, \buf_reg
.endm


/**
 * Save an interrupted user context and switch to the kernel context
 *
 * \param exception_type  kernel name of exception type
 * \param pc_adjust       value that gets subtracted from saved user PC
 */
.macro _user_to_kernel_pic exception_type, pc_adjust

	/* disable fast interrupts when not in fast-interrupt mode */
	.if \exception_type != FIQ_TYPE
		cpsid f
	.endif

	/*
	 * The sp in svc mode still contains the base of the globally mapped buffer
	 * of this CPU. Hence go to svc mode, buffer user r0-r2, and make
	 * buffer pointer available to all modes
	 */
	.if \exception_type != RST_TYPE && \exception_type != SVC_TYPE
		cps #SVC_MODE
	.endif
	stm sp, {r0-r2}^
	mov r0, sp

	/* switch back to previous privileged mode */
	.if \exception_type == UND_TYPE
		cps #UND_MODE
	.endif
	.if \exception_type == PAB_TYPE
		cps #ABT_MODE
	.endif
	.if \exception_type == DAB_TYPE
		cps #ABT_MODE
	.endif
	.if \exception_type == IRQ_TYPE
		cps #IRQ_MODE
	.endif
	.if \exception_type == FIQ_TYPE
		cps #FIQ_MODE
	.endif

	/* switch to kernel protection-domain */
	adr sp, _mt_master_context_begin
	add sp, #TRANSIT_TTBR0_OFFSET
	ldm sp, {r1, r2, sp}
	_switch_protection_domain r1, r2, sp

	/* get user context-pointer */
	_get_client_context_ptr sp, r1, _mt_client_context_ptr

	/* adjust and save user pc */
	.if \pc_adjust != 0
		sub lr, lr, #\pc_adjust
	.endif
	str lr, [sp, #PC_OFFSET]

	/* restore user r0-r2 from buffer and save user r0-r12 */
	mov lr, r0
	ldm lr, {r0-r2}
	stm sp, {r0-r12}^

	/* save user sp and user lr */
	add r0, sp, #SP_OFFSET
	stm r0, {sp, lr}^

	/* get user psr and type of exception that interrupted the user */
	mrs r0, spsr
	mov r1, #\exception_type

	b _common_user_to_kernel_pic

.endm /* _user_to_kernel_pic */


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

	/***********************
	 ** Exception entries **
	 ***********************/

	b _rst_entry /* 0x00: reset                  */
	b _und_entry /* 0x04: undefined instruction  */
	b _svc_entry /* 0x08: supervisor call        */
	b _pab_entry /* 0x0c: prefetch abort         */
	b _dab_entry /* 0x10: data abort             */
	nop          /* 0x14: reserved               */
	b _irq_entry /* 0x18: interrupt request      */

	/******************************************************
	 ** Entry for fast interrupt requests at offset 0x1c **
	 ******************************************************/

	/* load the saved PSR of the the previous mode */
	mrs r8, spsr

	/* get the M bitfield from the read PSR value */
	and r9, r8, #0b11111

	/* skip following instructions if previous mode was user mode */
	cmp r9, #USR_MODE
	beq 1f

	/*
	 * If we reach this point, the previous mode was not the user
	 * mode, meaning an exception entry has been preempted by this
	 * fast interrupt before it could disable fast interrupts.
	 */

	/* disable fast interrupts in PSR value of previous mode */
	orr r8, #0b1000000

	/* apply PSR of previous mode */
	msr spsr_cxsf, r8

	/*
	 * Resume excecution of previous exception entry leaving the fast
	 * interrupt unhandled till fast interrupts get enabled again.
	 */
	subs pc, lr, #4

	/* switch to kernel to handle the fast interrupt */
	1:
	_user_to_kernel_pic FIQ_TYPE, FIQ_PC_ADJUST

	/***************************************************************
	 ** Code that switches from a non-FIQ exception to the kernel **
	 ***************************************************************/

	_rst_entry: _user_to_kernel_pic RST_TYPE, RST_PC_ADJUST
	_und_entry: _user_to_kernel_pic UND_TYPE, UND_PC_ADJUST
	_svc_entry: _user_to_kernel_pic SVC_TYPE, SVC_PC_ADJUST
	_pab_entry: _user_to_kernel_pic PAB_TYPE, PAB_PC_ADJUST
	_dab_entry: _user_to_kernel_pic DAB_TYPE, DAB_PC_ADJUST
	_irq_entry: _user_to_kernel_pic IRQ_TYPE, IRQ_PC_ADJUST

	/**************************************************************
	 ** Kernel-entry code that is common for all user exceptions **
	 **************************************************************/

	_common_user_to_kernel_pic:

	/* save user psr and type of exception that interrupted the user */
	add sp, sp, #PSR_OFFSET
	stm sp, {r0, r1}

	/*
	 * Clear exclusive access in local monitor,
	 * as we use strex/ldrex in our cmpxchg method, we've to do this during
	 * a context switch (ARM Reference Manual paragraph 3.4.4.)
	 */
	clrex

	/*
	 * Switch to supervisor mode to circumvent incorrect behavior of
	 * kernel high-level code in fast interrupt mode and to ensure that
	 * we're in svc mode at kernel exit. The latter is because kernel
	 * exit stores a buffer pointer into its banked sp that is also
	 * needed by the subsequent kernel entry.
	 */
	cps #SVC_MODE

	/* apply kernel sp */
	adr r0, _mt_master_context_begin
	_restore_kernel_sp r0, r1, r2

	/* apply kernel lr and kernel pc */
	add r1, r0, #LR_OFFSET
	ldm r1, {lr, pc}

	_mt_local_variables

	/****************************************************************
	 ** Code that switches from a kernel context to a user context **
	 ****************************************************************/

	.p2align 2
	.global _mt_user_entry_pic
	_mt_user_entry_pic:

	/* get user context and globally mapped buffer of this CPU */
	_get_client_context_ptr lr, r0, _mt_client_context_ptr
	_get_buffer_ptr sp, r0

	/* load user psr in spsr */
	ldr r0, [lr, #PSR_OFFSET]
	msr spsr_cxsf, r0

	/* apply banked user sp, banked user lr, and user r0-r12 */
	add r0, lr, #SP_OFFSET
	ldm r0, {sp, lr}^
	ldm lr, {r0-r12}^

	/* buffer user r0-r1, and user pc */
	stm sp, {r0, r1}
	ldr r0, [lr, #PC_OFFSET]
	str r0, [sp, #2*4]

	/* switch to user protection-domain */
	adr r0, _mt_master_context_begin
	ldr r0, [r0, #TRANSIT_TTBR0_OFFSET]
	add lr, lr, #CIDR_OFFSET
	ldm lr, {r1, lr}
	_switch_protection_domain r0, r1, lr

	/* apply user r0-r1 and user pc which implies application of spsr */
	ldm sp, {r0, r1, pc}^

	/* end of the mode transition code */
	.global _mt_end
	_mt_end:
