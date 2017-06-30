/*
 * \brief  TrustZone monitor mode exception vector
 * \author Stefan Kalkowski
 * \date   2015-02-16
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/*********************
 ** Constant values **
 *********************/

.set RST_TYPE, 1
.set UND_TYPE, 2
.set SVC_TYPE, 3
.set PAB_TYPE, 4
.set DAB_TYPE, 5
.set IRQ_TYPE, 6
.set FIQ_TYPE, 7
.set LR_OFFSET, 14 * 4


/**
 * Switch from normal into secure world
 *
 * \param exception_type  immediate exception type ID
 * \param pc_adjust       immediate value that gets subtracted from the
 *                        vm's PC before it gets saved
 */
.macro _nonsecure_to_secure exception_type, pc_adjust
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


.section .text
.p2align 12
.global monitor_mode_exception_vector
monitor_mode_exception_vector:
	b _mon_rst_entry                  /* reset                  */
	b _mon_und_entry                  /* undefined instruction  */
	b _mon_svc_entry                  /* supervisor call        */
	b _mon_pab_entry                  /* prefetch abort         */
	b _mon_dab_entry                  /* data abort             */
	nop                               /* reserved               */
	b _mon_irq_entry                  /* interrupt request      */
	_nonsecure_to_secure FIQ_TYPE, 4  /* fast interrupt request */

	_mon_rst_entry: _nonsecure_to_secure RST_TYPE, 0
	_mon_und_entry: _nonsecure_to_secure UND_TYPE, 4
	_mon_svc_entry: _nonsecure_to_secure SVC_TYPE, 0
	_mon_pab_entry: _nonsecure_to_secure PAB_TYPE, 4
	_mon_dab_entry: _nonsecure_to_secure DAB_TYPE, 8
	_mon_irq_entry: _nonsecure_to_secure IRQ_TYPE, 4

	_nonsecure_kernel_entry:
	ldr   lr, [sp, #17*4]         /* load kernel sp from vm context  */
	stmia r0!, {r1-r2}            /* save spsr, and exception reason */
	mrc   p15, 0, r3, c6, c0, 0   /* move DFAR  to r3                */
	mrc   p15, 0, r4, c2, c0, 0   /* move TTBR0 to r4                */
	mrc   p15, 0, r5, c2, c0, 1   /* move TTBR1 to r5                */
	mrc   p15, 0, r6, c2, c0, 2   /* move TTBRC to r6                */
	mov   r1, #0
	mcr   p15, 0, r1, c1, c1, 0   /* disable non-secure bit          */
	.irp mode,27,19,23,18,17      /* save mode specific registers    */
		cps   #\mode              /* switch to given mode            */
		mrs   r1, spsr            /* store mode-specific spsr        */
		stmia r0!, {r1,sp,lr}     /* store mode-specific sp and lr   */
	.endr
	stmia r0!, {r8-r12}           /* save fiq r8-r12                 */
	stmia r0!, {r3-r6}            /* save MMU registers              */
	cps   #22                     /* switch back to monitor mode     */
	mov   r0, #0b111010011        /* spsr to SVC mode, irqs masked   */
	msr   spsr_cxsf, r0
	mov   r1, lr
	cps   #19
	mov   sp, r1
	cps   #22
	adr   lr, _kernel_entry
	ldr   lr, [lr]
	subs  pc, lr, #0              /* jump back into kernel           */

	_kernel_entry: .long kernel


/* jump to this point to switch to TrustZone's normal world */
.global monitor_mode_enter_normal_world
monitor_mode_enter_normal_world:
	cps   #22                     /* switch to monitor mode          */
	mov   sp, r0                  /* store vm context pointer        */
	mov   lr, r1                  /* store kernel sp temporarily     */
	add   r0, r0, #18*4           /* add offset of banked modes      */
	.irp mode,27,19,23,18,17      /* save mode specific registers    */
		cps   #\mode              /* switch to given mode            */
		ldmia r0!, {r2,sp,lr}     /* load mode's sp, lr, and spsr    */
		msr   spsr_cxfs, r2       /* load mode's spsr                */
	.endr
	ldmia r0!, {r8 - r12}         /* load fiq r8-r12                 */
	cps   #22                     /* switch to monitor mode          */
	ldmia sp, {r0-lr}^            /* load user r0-r12,sp,lr          */
	str   lr, [sp, #17*4]         /* store kernel sp in vm context   */
	ldr   lr, [sp, #16*4]         /* load vm's cpsr to lr            */
	msr   spsr_cxfs, lr           /* save cpsr to be load            */
	mov   lr, #13
	mcr   p15, 0, lr, c1, c1, 0   /* enable EA, FIQ, NS bit in SCTRL */
	ldr   lr, [sp, #15*4]         /* load vm's ip                    */
	subs  pc, lr, #0
