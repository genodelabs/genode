/*
 * \brief  Transition between kernel/userland, and secure/non-secure world
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-11-15
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.include "macros.s"


/***************
 ** Constants **
 ***************/

/* hardware names of processor modes */
.set usr_mode, 16
.set fiq_mode, 17
.set irq_mode, 18
.set svc_mode, 19
.set abt_mode, 23
.set und_mode, 27

/* size of local variables */
.set buffer_size, 2 * 4

/* common constants */
_mt_constants
_common_constants


/************
 ** Macros **
 ************/

/**
 * Determine the base of the client context of the executing processor
 *
 * \param target_reg  register that shall receive the base pointer
 * \param buf_reg     register that can be polluted by the macro
 */
.macro _get_client_context_ptr target_reg, buf_reg

	/* get kernel name of processor */
	_get_processor_id \buf_reg

	/* multiply processor name with pointer size to get offset of pointer */
	mov \target_reg, #context_ptr_size
	mul \buf_reg, \buf_reg, \target_reg

	/* get base of the pointer array */
	adr \target_reg, _mt_client_context_ptr

	/* add offset and base to get processor-local pointer */
	add \target_reg, \target_reg, \buf_reg
	ldr \target_reg, [\target_reg]
.endm


/**
 * Determine the base of the globally mapped buffer of the executing processor
 *
 * \param target_reg  register that shall receive the base pointer
 * \param buf_reg     register that can be polluted by the macro
 */
.macro _get_buffer_ptr target_reg, buf_reg

	/* get kernel name of processor */
	_get_processor_id \buf_reg

	/* multiply processor name with buffer size to get offset of buffer */
	mov \target_reg, #buffer_size
	mul \buf_reg, \buf_reg, \target_reg

	/* get base of the buffer array */
	adr \target_reg, _mt_buffer

	/* add offset and base to get processor-local buffer */
	add \target_reg, \target_reg, \buf_reg
.endm


/**
 * Compose a value for the translation-table-base register 0 and apply it
 *
 * \param section_table_reg  register that contains targeted section-table base
 */
.macro _init_ttbr0 section_table_reg

	/* IRGN bitfield is set to 1 to compose the TTBR0 value */
	orr \section_table_reg, \section_table_reg, #0b1000000

	/* write translation-table-base register 0 */
	mcr p15, 0, \section_table_reg, c2, c0, 0

	/* instruction and data synchronization barrier */
	isb
	dsb
.endm


/**
 * Apply a value to the CONTEXTIDR register
 *
 * \param  contexidr_reg  register that contains the new CONTEXTIDR value
 */
.macro _init_contextidr contextidr_reg

	/* write CONTEXTIDR register */
	mcr p15, 0, \contextidr_reg, c13, c0, 1

	/* finish all previous instructions */
	isb
.endm


/**
 * Save an interrupted user context and switch to the kernel context
 *
 * \param exception_type  kernel name of exception type
 * \param pc_adjust       value that gets subtracted from saved user PC
 */
.macro _user_to_kernel_pic exception_type, pc_adjust

	/*************************************************************************
	 ** Still in user protection domain, thus avoid access to kernel memory **
	 *************************************************************************/

	/* disable fast interrupts when not in fast-interrupt mode */
	.if \exception_type != fiq_type
		cpsid f
	.endif

	/*
	 * The sp in svc mode still contains the base of the globally mapped
	 * buffer of this processor. Hence go to svc mode and buffer user r0 and
	 * user r1 to globally mapped memory to be able to pollute r0 and r1.
	 */
	.if \exception_type != rst_type && \exception_type != svc_type
		cps #svc_mode
	.endif
	stm sp, {r0, r1}^

	/* make buffer pointer available to all modes */
	mov r0, sp

	/* switch back to previous privileged mode */
	.if \exception_type == und_type
		cps #und_mode
	.endif
	.if \exception_type == pab_type
		cps #abt_mode
	.endif
	.if \exception_type == dab_type
		cps #abt_mode
	.endif
	.if \exception_type == irq_type
		cps #irq_mode
	.endif
	.if \exception_type == fiq_type
		cps #fiq_mode
	.endif

	/* load kernel contextidr and base of the kernel section-table */
	adr sp, _mt_master_context_begin
	add sp, #contextidr_offset
	ldm sp, {r1, sp}

	/* switch to kernel protection-domain */
	_init_contextidr r1
	_init_ttbr0 sp

	/*******************************************
	 ** Now it's save to access kernel memory **
	 *******************************************/

	/* get user context-pointer */
	_get_client_context_ptr sp, r1

	/* adjust and save user pc */
	.if \pc_adjust != 0
		sub lr, lr, #\pc_adjust
	.endif
	str lr, [sp, #pc_offset]

	/* move buffer pointer to lr to enable us to save user r0 - r12 via stm */
	mov lr, r0

	/* restore user r0 and user r1 */
	ldm lr, {r0, r1}

	/* save user r0 - r12 */
	stm sp, {r0-r12}^

	/* save user sp and user lr */
	add r0, sp, #sp_offset
	stm r0, {sp, lr}^

	/* get user psr and type of exception that interrupted the user */
	mrs r0, spsr
	mov r1, #\exception_type

	b _common_user_to_kernel_pic

.endm /* _user_to_kernel_pic */


/**
 * Save sp, lr and spsr register banks of specified exception mode
 */
.macro _save_bank mode
	cps   #\mode          /* switch to given mode                  */
	mrs   r1, spsr        /* store mode-specific spsr              */
	stmia r0!, {r1,sp,lr} /* store mode-specific sp and lr         */
.endm /* _save_bank mode */


/**
 * Switch from an interrupted VM to the kernel context
 *
 * \param exception_type  immediate exception type ID
 * \param pc_adjust       immediate value that gets subtracted from the
 *                        vm's PC before it gets saved
 */
.macro _vm_to_kernel exception_type, pc_adjust
	ldr   sp, _mt_client_context_ptr   /* load context pointer            */
	stmia sp, {r0-lr}^                 /* save user regs r0-r12,sp,lr     */
	add   r0, sp, #15*4
	.if \pc_adjust != 0                /* adjust pc if necessary          */
		sub lr, lr, #\pc_adjust
	.endif
	stmia r0!, {lr}                    /* save pc                         */
	mrs   r1, spsr                     /* spsr to r0                      */
	mov   r2, #\exception_type         /* exception reason to r1          */
	stmia r0!, {r1-r2}                 /* save spsr, and exception reason */
	mrc   p15, 0, r3, c6, c0, 0        /* move DFAR  to r3                */
	mrc   p15, 0, r4, c2, c0, 0        /* move TTBR0 to r4                */
	mrc   p15, 0, r5, c2, c0, 1        /* move TTBR1 to r5                */
	mrc   p15, 0, r6, c2, c0, 2        /* move TTBRC to r6                */
	mov   r1, #0
	mcr   p15, 0, r1, c1, c1, 0        /* disable non-secure bit          */
	_save_bank 27                      /* save undefined banks            */
	_save_bank 19                      /* save supervisor banks           */
	_save_bank 23                      /* save abort banks                */
	_save_bank 18                      /* save irq banks                  */
	_save_bank 17                      /* save fiq banks                  */
	stmia r0!, {r8-r12}                /* save fiq r8-r12                 */
	stmia r0!, {r3-r6}                 /* save MMU registers              */
	b _common_client_to_kernel_pic
.endm /* _vm_to_kernel */


/**
 * Restore sp, lr and spsr register banks of specified exception mode
 */
.macro _restore_bank mode
	cps   #\mode          /* switch to given mode                        */
	ldmia r0!, {r1,sp,lr} /* load mode-specific sp, lr, and spsr into r1 */
	msr   spsr_cxfs, r1   /* load mode-specific spsr                     */
.endm


/**
 * Switch from kernel context to a VM
 */
.macro _kernel_to_vm
	ldr   r0, _mt_client_context_ptr   /* get vm context pointer               */
	add   r0, r0, #18*4         /* add offset of banked modes           */
	_restore_bank 27            /* load undefined banks                 */
	_restore_bank 19            /* load supervisor banks                */
	_restore_bank 23            /* load abort banks                     */
	_restore_bank 18            /* load irq banks                       */
	_restore_bank 17            /* load fiq banks                       */
	ldmia r0!, {r8 - r12}       /* load fiq r8-r12                      */
	cps   #22                   /* switch to monitor mode               */
	ldr   sp, _mt_client_context_ptr   /* get vm context pointer               */
	ldmia sp, {r0-lr}^          /* load user r0-r12,sp,lr               */
	ldr   lr, [sp, #16*4]       /* load vm's cpsr to lr                 */
	msr   spsr_cxfs, lr         /* save cpsr to be load when switching  */
	mov   lr, #13
	mcr   p15, 0, lr, c1, c1, 0 /* enable EA, FIQ, and NS bit in SCTRL  */
	ldr   lr, [sp, #15*4]       /* load vm's ip                         */
	subs  pc, lr, #0
.endm /* _kernel_to_vm */


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
	.p2align min_page_size_log2
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
	cmp r9, #usr_mode
	beq 1f

	/*
	 * If we reach this point, the previous mode was not the user
	 * mode, meaning an exception entry has been preempted by this
	 * fast interrupt before it could disable fast interrupts.
	 */

	/* disable fast interrupts in PSR value of previous mode */
	orr r8, #0b1000000

	/* apply PSR of previous mode */
	msr spsr, r8

	/*
	 * Resume excecution of previous exception entry leaving the fast
	 * interrupt unhandled till fast interrupts get enabled again.
	 */
	subs pc, lr, #4

	/* switch to kernel to handle the fast interrupt */
	1:
	_user_to_kernel_pic fiq_type, fiq_pc_adjust

	/***************************************************************
	 ** Code that switches from a non-FIQ exception to the kernel **
	 ***************************************************************/

	_rst_entry: _user_to_kernel_pic rst_type, rst_pc_adjust
	_und_entry: _user_to_kernel_pic und_type, und_pc_adjust
	_svc_entry: _user_to_kernel_pic svc_type, svc_pc_adjust
	_pab_entry: _user_to_kernel_pic pab_type, pab_pc_adjust
	_dab_entry: _user_to_kernel_pic dab_type, dab_pc_adjust
	_irq_entry: _user_to_kernel_pic irq_type, irq_pc_adjust

	/**************************************************************
	 ** Kernel-entry code that is common for all user exceptions **
	 **************************************************************/

	_common_user_to_kernel_pic:

	/* save user psr and type of exception that interrupted the user */
	add sp, sp, #psr_offset
	stm sp, {r0, r1}

	/*********************************************************
	 ** Kernel-entry code that is common for all exceptions **
	 *********************************************************/

	_common_client_to_kernel_pic:

	/*
	 * Switch to supervisor mode to circumvent incorrect behavior of
	 * kernel high-level code in fast interrupt mode and to ensure that
	 * we're in svc mode at kernel exit. The latter is because kernel
	 * exit stores a buffer pointer into its banked sp that is also
	 * needed by the subsequent kernel entry.
	 */
	cps #svc_mode

	/* get base of the kernel-stacks area and the kernel-stack size */
	adr r0, _mt_master_context_begin
	add r1, r0, #r12_offset
	ldm r1, {r2, r3}

	/* determine top of the kernel stack of this processor and apply it as SP */
	_init_kernel_sp r3, r2

	/* apply kernel lr and kernel pc */
	add r1, r0, #lr_offset
	ldm r1, {lr, pc}

	_mt_local_variables

	/****************************************************************
	 ** Code that switches from a kernel context to a user context **
	 ****************************************************************/

	.p2align 2
	.global _mt_user_entry_pic
	_mt_user_entry_pic:

	/* get user context and globally mapped buffer of this processor */
	_get_client_context_ptr lr, r0
	_get_buffer_ptr sp, r0

	/* buffer user pc and base of user section-table globally mapped */
	ldr r0, [lr, #pc_offset]
	ldr r1, [lr, #section_table_offset]
	stm sp, {r0, r1}

	/* buffer user psr in spsr */
	ldr r0, [lr, #psr_offset]
	msr spsr, r0

	/* setup banked user sp and banked user lr */
	add r0, lr, #sp_offset
	ldm r0, {sp, lr}^

	/* setup user r0 to r12 */
	ldm lr, {r0-r12}^

	/* load user contextidr */
	ldr lr, [lr, #contextidr_offset]

	/********************************************************
	 ** From now on, until we leave kernel mode, we must   **
	 ** avoid access to memory that is not mapped globally **
	 ********************************************************/

	/* switch to user protection-domain */
	_init_contextidr lr
	ldr lr, [sp, #4]
	_init_ttbr0 lr

	/* apply user pc which implies application of spsr as user psr */
	ldm sp, {pc}^

	/* end of the mode transition code */
	.global _mt_end
	_mt_end:

	/*
	 * On vm exceptions the CPU has to jump to one of the following
	 * 7 entry vectors to switch to a kernel context.
	 */
	.p2align 4
	.global _mon_kernel_entry
	_mon_kernel_entry:
		b _mon_rst_entry           /* reset                  */
		b _mon_und_entry           /* undefined instruction  */
		b _mon_svc_entry           /* supervisor call        */
		b _mon_pab_entry           /* prefetch abort         */
		b _mon_dab_entry           /* data abort             */
		nop                        /* reserved               */
		b _mon_irq_entry           /* interrupt request      */
		_vm_to_kernel fiq_type, 4  /* fast interrupt request */

		/* PICs that switch from a vm exception to the kernel */
		_mon_rst_entry: _vm_to_kernel rst_type, 0
		_mon_und_entry: _vm_to_kernel und_type, 4
		_mon_svc_entry: _vm_to_kernel svc_type, 0
		_mon_pab_entry: _vm_to_kernel pab_type, 4
		_mon_dab_entry: _vm_to_kernel dab_type, 8
		_mon_irq_entry: _vm_to_kernel irq_type, 4

	/* kernel must jump to this point to switch to a vm */
	.p2align 2
	.global _mt_vm_entry_pic
	_mt_vm_entry_pic:
		_kernel_to_vm
