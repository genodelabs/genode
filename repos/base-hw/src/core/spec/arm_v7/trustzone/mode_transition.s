/*
 * \brief  Transition between secure/normal worl
 * \author Stefan Kalkowski
 * \date   2015-02-16
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
.include "macros.s"

/**
 * Switch from nonsecure into secure world
 *
 * \param exception_type  immediate exception type ID
 * \param pc_adjust       immediate value that gets subtracted from the
 *                        vm's PC before it gets saved
 */
.macro _nonsecure_to_secure exception_type, pc_adjust
	ldr   sp, _tz_client_context   /* load context pointer            */
	stmia sp, {r0-lr}^             /* save user regs r0-r12,sp,lr     */
	add   r0, sp, #15*4
	.if \pc_adjust != 0            /* adjust pc if necessary          */
		sub lr, lr, #\pc_adjust
	.endif
	stmia r0!, {lr}                /* save pc                         */
	mrs   r1, spsr                 /* spsr to r0                      */
	mov   r2, #\exception_type     /* exception reason to r1          */
	b     _nonsecure_kernel_entry
.endm /* _non_to_secure */


/**
 * Switch from secure into nonsecure world
 */
.macro _secure_to_nonsecure
	ldr   r0, _tz_client_context /* get vm context pointer               */
	add   r0, r0, #18*4          /* add offset of banked modes           */
	_restore_bank 27             /* load undefined banks                 */
	_restore_bank 19             /* load supervisor banks                */
	_restore_bank 23             /* load abort banks                     */
	_restore_bank 18             /* load irq banks                       */
	_restore_bank 17             /* load fiq banks                       */
	ldmia r0!, {r8 - r12}        /* load fiq r8-r12                      */
	cps   #22                    /* switch to monitor mode               */
	ldr   sp, _tz_client_context /* get vm context pointer               */
	ldmia sp, {r0-lr}^           /* load user r0-r12,sp,lr               */
	ldr   lr, [sp, #16*4]        /* load vm's cpsr to lr                 */
	msr   spsr_cxfs, lr          /* save cpsr to be load when switching  */
	mov   lr, #13
	mcr   p15, 0, lr, c1, c1, 0  /* enable EA, FIQ, and NS bit in SCTRL  */
	ldr   lr, [sp, #15*4]        /* load vm's ip                         */
	subs  pc, lr, #0
.endm /* _secure_to_nonsecure */


.section .text

/*
 * On TrustZone exceptions the CPU has to jump to one of the following
 * 7 entry vectors to switch to a kernel context.
 */
.p2align MIN_PAGE_SIZE_LOG2
.global _mon_kernel_entry
_mon_kernel_entry:
	b _mon_rst_entry           /* reset                  */
	b _mon_und_entry           /* undefined instruction  */
	b _mon_svc_entry           /* supervisor call        */
	b _mon_pab_entry           /* prefetch abort         */
	b _mon_dab_entry           /* data abort             */
	nop                        /* reserved               */
	b _mon_irq_entry           /* interrupt request      */
	_nonsecure_to_secure FIQ_TYPE, 4  /* fast interrupt request */

	/* PICs that switch from a vm exception to the kernel */
	_mon_rst_entry: _nonsecure_to_secure RST_TYPE, 0
	_mon_und_entry: _nonsecure_to_secure UND_TYPE, 4
	_mon_svc_entry: _nonsecure_to_secure SVC_TYPE, 0
	_mon_pab_entry: _nonsecure_to_secure PAB_TYPE, 4
	_mon_dab_entry: _nonsecure_to_secure DAB_TYPE, 8
	_mon_irq_entry: _nonsecure_to_secure IRQ_TYPE, 4

/* space for a copy of the kernel context */
.p2align 2
.global _tz_master_context
_tz_master_context:
.space 32 * 4

/* space for a client context-pointer */
.p2align 2
.global _tz_client_context
_tz_client_context:
	.space CONTEXT_PTR_SIZE

_nonsecure_kernel_entry:
	stmia r0!, {r1-r2}          /* save spsr, and exception reason */
	mrc   p15, 0, r3, c6, c0, 0 /* move DFAR  to r3                */
	mrc   p15, 0, r4, c2, c0, 0 /* move TTBR0 to r4                */
	mrc   p15, 0, r5, c2, c0, 1 /* move TTBR1 to r5                */
	mrc   p15, 0, r6, c2, c0, 2 /* move TTBRC to r6                */
	mov   r1, #0
	mcr   p15, 0, r1, c1, c1, 0 /* disable non-secure bit          */
	_save_bank 27               /* save undefined banks            */
	_save_bank 19               /* save supervisor banks           */
	_save_bank 23               /* save abort banks                */
	_save_bank 18               /* save irq banks                  */
	_save_bank 17               /* save fiq banks                  */
	stmia r0!, {r8-r12}         /* save fiq r8-r12                 */
	stmia r0!, {r3-r6}          /* save MMU registers              */
	cps #SVC_MODE
	adr r0, _tz_master_context
	_restore_kernel_sp r0, r1, r2 /* apply kernel sp */
	add r1, r0, #LR_OFFSET
	ldm r1, {lr, pc}

/* kernel must jump to this point to switch to a vm */
.global _mt_nonsecure_entry_pic
_mt_nonsecure_entry_pic:
	_secure_to_nonsecure
