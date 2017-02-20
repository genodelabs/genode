/*
 * \brief  Transition between virtual/host mode
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

.macro _vm_exit exception_type
	str   r0, [sp]
	mrc   p15, 4, r0, c1, c1, 0 /* read HCR register */
	tst   r0, #1                /* check VM bit      */
	ldreq r0, [sp]
	beq   _host_to_vm
	mov   r0, #\exception_type
	str   r0, [sp, #17*4]
	b     _vm_to_host
.endm /* _vm_exit */


.section .text

/*
 * On virtualization exceptions the CPU has to jump to one of the following
 * 7 entry vectors to switch to a kernel context.
 */
.p2align 12
.global _vt_host_entry
_vt_host_entry:
	b _vt_rst_entry
	b _vt_und_entry    /* undefined instruction  */
	b _vt_svc_entry    /* hypervisor call        */
	b _vt_pab_entry    /* prefetch abort         */
	b _vt_dab_entry    /* data abort             */
	b _vt_trp_entry    /* hypervisor trap        */
	b _vt_irq_entry    /* interrupt request      */
	_vm_exit 7          /* fast interrupt request */

_vt_rst_entry: _vm_exit 1
_vt_und_entry: _vm_exit 2
_vt_svc_entry: _vm_exit 3
_vt_pab_entry: _vm_exit 4
_vt_dab_entry: _vm_exit 5
_vt_irq_entry: _vm_exit 6
_vt_trp_entry: _vm_exit 8

/* space for a copy of the host context */
.p2align 2
.global _vt_host_context_ptr
_vt_host_context_ptr:
	.space CONTEXT_PTR_SIZE

/* space for a vm context-pointer per CPU */
.p2align 2
.global _vt_vm_context_ptr
_vt_vm_context_ptr:
.rept NR_OF_CPUS
	.space CONTEXT_PTR_SIZE
.endr

_host_to_vm:
	msr   elr_hyp,   r2
	msr   spsr_cxfs, r3           /* load cpsr              */
	mcrr  p15, 6, r5, r6, c2      /* write VTTBR            */
	mcr   p15, 0, r7, c1, c0, 0   /* write SCTRL            */
	mcr   p15, 4, r8, c1, c1, 3   /* write HSTR             */
	mcr   p15, 4, r9, c1, c1, 0   /* write HCR register     */
	mcr   p15, 0, r12, c2, c0, 2  /* write TTBRC            */
	sub   sp, r0, #46*4
	ldm   r0!, {r1-r12}
	mcr   p15, 0, r1, c2, c0, 0   /* write TTBR0            */
	mcr   p15, 0, r2, c2, c0, 1   /* write TTBR1            */
	mcr   p15, 0, r3, c10, c2, 0  /* write PRRR             */
	mcr   p15, 0, r4, c10, c2, 1  /* write NMRR             */
	mcr   p15, 0, r5, c3, c0, 0   /* write DACR             */
	mcr   p15, 0, r6, c5, c0, 0   /* write DFSR             */
	mcr   p15, 0, r7, c5, c0, 1   /* write IFSR             */
	mcr   p15, 0, r8, c5, c1, 0   /* write ADFSR            */
	mcr   p15, 0, r9, c5, c1, 1   /* write AIFSR            */
	mcr   p15, 0, r10, c6, c0, 0  /* write DFAR             */
	mcr   p15, 0, r11, c6, c0, 2  /* write IFAR             */
	mcr   p15, 0, r12, c13, c0, 1 /* write CIDR             */
	ldm   r0, {r1-r4}
	mcr   p15, 0, r1, c13, c0, 2  /* write TLS1             */
	mcr   p15, 0, r2, c13, c0, 3  /* write TLS2             */
	mcr   p15, 0, r3, c13, c0, 4  /* write TLS3             */
	mcr   p15, 0, r4, c1,  c0, 2  /* write CPACR            */
	ldmia sp, {r0-r12}            /* load vm's r0-r12       */
	eret

_vm_to_host:
	add r0, sp, #1*4
	stmia r0, {r1-r12}            /* save regs r1-r12       */
	mov r1, #0
	mcrr p15, 6, r1, r1, c2       /* write VTTBR            */
	mcr p15, 4, r1, c1, c1, 0     /* write HCR register     */
	mcr p15, 4, r1, c1, c1, 3     /* write HSTR register    */
	mcr p15, 0, r1, c1, c0, 2     /* write CPACR            */
	mrs r1, ELR_hyp               /* read ip                */
	mrs r2, spsr                  /* read cpsr              */
	mrc p15, 0, r3, c1, c0, 0     /* read SCTRL             */
	mrc p15, 4, r4, c5, c2, 0     /* read HSR               */
	mrc p15, 4, r5, c6, c0, 4     /* read HPFAR             */
	mrc p15, 4, r6, c6, c0, 0     /* read HDFAR             */
	mrc p15, 4, r7, c6, c0, 2     /* read HIFAR             */
	mrc p15, 0, r8, c2, c0, 2     /* read TTBRC             */
	mrc p15, 0, r9, c2, c0, 0     /* read TTBR0             */
	mrc p15, 0, r10, c2, c0, 1    /* read TTBR1             */
	add r0, sp, #40*4             /* offset SCTRL           */
	stm r0!, {r3-r10}
	add r0, r0, #3*4
	mrc p15, 0, r3, c5, c0, 0     /* read DFSR              */
	mrc p15, 0, r4, c5, c0, 1     /* read IFSR              */
	mrc p15, 0, r5, c5, c1, 0     /* read ADFSR             */
	mrc p15, 0, r6, c5, c1, 1     /* read AIFSR             */
	mrc p15, 0, r7, c6, c0, 0     /* read DFAR              */
	mrc p15, 0, r8, c6, c0, 2     /* read IFAR              */
	mrc p15, 0, r9, c13, c0, 1    /* read CIDR              */
	mrc p15, 0, r10, c13, c0, 2   /* read TLS1              */
	mrc p15, 0, r11, c13, c0, 3   /* read TLS2              */
	mrc p15, 0, r12, c13, c0, 4   /* read TLS3              */
	stm r0, {r3-r12}
	add r0, sp, #13*4
	ldr r3, _vt_host_context_ptr
	_restore_kernel_sp r3, r4, r5
	add r3, r3, #CIDR_OFFSET
	ldmia r3, {r4-r9}
	_switch_protection_domain r0, r4, r5
	mcr p15, 0, r6, c1, c0, 0     /* write SCTRL            */
	mcr p15, 0, r7, c2, c0, 2     /* write TTBRC            */
	mcr p15, 0, r8, c10, c2, 0    /* write MAIR0            */
	mcr p15, 0, r9, c3, c0, 0     /* write DACR             */
	cps #SVC_MODE
	stmia r0, {r13-r14}^          /* save user regs sp,lr   */
	add r0, r0, #2*4
	stmia r0!, {r1-r2}            /* save ip, cpsr          */
	add r0, r0, #1*4
	_save_bank UND_MODE           /* save undefined banks   */
	_save_bank SVC_MODE           /* save supervisor banks  */
	_save_bank ABT_MODE           /* save abort banks       */
	_save_bank IRQ_MODE           /* save irq banks         */
	_save_bank FIQ_MODE           /* save fiq banks         */
	stmia r0!, {r8-r12}           /* save fiq r8-r12        */
	cps #SVC_MODE
	ldr r0, _vt_host_context_ptr
	_restore_kernel_sp r0, r1, r2 /* apply host kernel sp   */
	add r1, r0, #LR_OFFSET        /* apply host kernel lr   */
	ldm r1, {lr, pc}

/* host kernel must jump to this point to switch to a vm */
.global _vt_vm_entry
_vt_vm_entry:
	_get_client_context_ptr r0, lr, _vt_vm_context_ptr
	add   r0, r0, #SP_OFFSET
	ldm   r0, {r13 - r14}^
	add   r0, r0, #2*4
	ldmia r0!, {r2 - r4}
	_restore_bank UND_MODE
	_restore_bank SVC_MODE
	_restore_bank ABT_MODE
	_restore_bank IRQ_MODE
	_restore_bank FIQ_MODE
	ldmia r0!, {r8 - r12}
	cps   #SVC_MODE
	ldm   r0!, {r5 - r12}
	hvc   #0
