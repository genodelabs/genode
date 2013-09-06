/*
 * \brief  Transition between kernel/userland, and secure/non-secure world
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-11-15
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


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

	/* when not in FIQ mode disable FIQs */
	.if \exception_type != 6
		cpsid f
	.endif

	/************************************************
	 ** We're still in the user protection domain, **
	 ** so we must avoid access to kernel memory   **
	 ************************************************/

	/* load kernel cidr */
	adr sp, _mt_master_context_begin
	ldr sp, [sp, #18*4]
	mcr p15, 0, sp, c13, c0, 1
	isb

	/* load kernel section table */
	adr sp, _mt_master_context_begin
	ldr sp, [sp, #19*4]
	orr sp, sp, #0b1000000 /* set TTBR0 flags */
	mcr p15, 0, sp, c2, c0, 0
	isb
	dsb

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
	str r0, [sp, #17*4]

	/*
	 * Switch to supervisor mode
	 * FIXME This is done due to incorrect behavior when running the kernel
	 *       high-level-code in FIQ-exception mode. Please debug this behavior
	 *       and remove this switch.
	 */
	cps #19

	/* get kernel context pointer */
	adr r0, _mt_master_context_begin

	/* load kernel context */
	add r0, r0, #13*4
	ldmia r0, {sp, lr, pc}

.endm /* _user_to_kernel_pic */


/**
 * Switch from kernel context to a user context
 */
.macro _kernel_to_user_pic

	/* get user context pointer */
	ldr lr, _mt_client_context_ptr

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
	ldr sp, [lr, #18*4]
	ldr lr, [lr, #19*4]
	orr lr, lr, #0b1000000 /* set TTBR0 flags */

	/********************************************************
	 ** From now on, until we leave kernel mode, we must   **
	 ** avoid access to memory that is not mapped globally **
	 ********************************************************/

	/* apply user contextidr and section table */
	mcr p15, 0, sp, c13, c0, 1
	mcr p15, 0, lr, c2, c0, 0
	isb
	dsb

	/* load user pc (implies application of the user psr) */
	adr lr, _mt_buffer
	ldmia lr, {pc}^

.endm /* _kernel_to_user_pic */


.macro _fiq_check_prior_mode
	mrs   r8, spsr    /* load fiq-spsr                        */
	and   r8, #31
	cmp   r8, #16     /* check whether we come from user-mode */
	beq   1f
	mrs   r8, spsr    /* enable fiq-ignore bit                */
	orr   r8, #64
	msr   spsr, r8
	subs  pc, lr, #4  /* resume previous exception            */
	1:
.endm /* _fiq_check_prior_mode */

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
	ldr   sp, _mt_client_context_ptr          /* load context pointer            */
	stmia sp, {r0-lr}^                 /* save user regs r0-r12,sp,lr     */
	add   r0, sp, #15*4
	.if \pc_adjust != 0                /* adjust pc if necessary          */
		sub lr, lr, #\pc_adjust
	.endif
	stmia r0!, {lr}                    /* save pc                         */
	mrs   r1, spsr                     /* spsr to r0                      */
	mov   r2, #\exception_type         /* exception reason to r1          */
	stmia r0!, {r1-r2}                 /* save spsr, and exception reason */
	mov   r1, #0
	mcr   p15, 0, r1, c1, c1, 0        /* disable non-secure bit          */
	_save_bank 27                      /* save undefined banks            */
	_save_bank 19                      /* save supervisor banks           */
	_save_bank 23                      /* save abort banks                */
	_save_bank 18                      /* save irq banks                  */
	_save_bank 17                      /* save fiq banks                  */
	stmia r0!, {r8-r12}                /* save fiq r8-r12                 */
	cps   #19                          /* switch to supervisor mode       */
	adr   r0, _mt_master_context_begin /* get kernel context pointer      */
	add   r0, r0, #13*4                /* load kernel context             */
	ldmia r0, {sp,lr,pc}
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


.section .text

	/*
	 * The mode transition PIC switches between a kernel context and a user
	 * context and thereby between their address spaces. Due to the latter
	 * it must be mapped executable to the same region in every address space.
	 * To enable such switching, the kernel context must be stored within this
	 * region, thus one should map it solely accessable for privileged modes.
	 */
	.p2align 12 /* page-aligned */
	.global _mt_begin
	_mt_begin:

		/*
		 * On user exceptions the CPU has to jump to one of the following
		 * 7 entry vectors to switch to a kernel context.
		 */
		.global _mt_kernel_entry_pic
		_mt_kernel_entry_pic:

			b _rst_entry              /* 0x00: reset                  */
			b _und_entry              /* 0x04: undefined instruction  */
			b _svc_entry              /* 0x08: supervisor call        */
			b _pab_entry              /* 0x0c: prefetch abort         */
			b _dab_entry              /* 0x10: data abort             */
			nop                       /* 0x14: reserved               */
			b _irq_entry              /* 0x18: interrupt request      */
			_fiq_check_prior_mode     /* 0x1c: fast interrupt request */
			_user_to_kernel_pic 7, 4

			/* PICs that switch from an user exception to the kernel */
			_rst_entry: _user_to_kernel_pic 1, 0
			_und_entry: _user_to_kernel_pic 2, 4
			_svc_entry: _user_to_kernel_pic 3, 0
			_pab_entry: _user_to_kernel_pic 4, 4
			_dab_entry: _user_to_kernel_pic 5, 8
			_irq_entry: _user_to_kernel_pic 6, 4

		/* kernel must jump to this point to switch to a user context */
		.p2align 2
		.global _mt_user_entry_pic
		_mt_user_entry_pic:
			_kernel_to_user_pic

		/* leave some space for the kernel context */
		.p2align 2
		.global _mt_master_context_begin
		_mt_master_context_begin: .space 32*4
		.global _mt_master_context_end
		_mt_master_context_end:

		/* pointer to the context backup space */
		.p2align 2
		.global _mt_client_context_ptr
		_mt_client_context_ptr: .long 0

		/* a local word-sized buffer */
		.p2align 2
		.global _mt_buffer
		_mt_buffer: .long 0

	.global _mt_end
	_mt_end:

	/*
	 * On vm exceptions the CPU has to jump to one of the following
	 * 7 entry vectors to switch to a kernel context.
	 */
	.p2align 4
	.global _mon_kernel_entry
	_mon_kernel_entry:
		b _mon_rst_entry    /* reset                  */
		b _mon_und_entry    /* undefined instruction  */
		b _mon_svc_entry    /* supervisor call        */
		b _mon_pab_entry    /* prefetch abort         */
		b _mon_dab_entry    /* data abort             */
		nop                 /* reserved               */
		b _mon_irq_entry    /* interrupt request      */
		_vm_to_kernel 7, 4  /* fast interrupt request */

		/* PICs that switch from a vm exception to the kernel */
		_mon_rst_entry: _vm_to_kernel 1, 0
		_mon_und_entry: _vm_to_kernel 2, 4
		_mon_svc_entry: _vm_to_kernel 3, 0
		_mon_pab_entry: _vm_to_kernel 4, 4
		_mon_dab_entry: _vm_to_kernel 5, 8
		_mon_irq_entry: _vm_to_kernel 6, 4

	/* kernel must jump to this point to switch to a vm */
	.p2align 2
	.global _mt_vm_entry_pic
	_mt_vm_entry_pic:
		_kernel_to_vm
