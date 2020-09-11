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

.macro _vm_exit exception_type
	push  { r0 }
	mrc   p15, 4, r0, c1, c1, 0 /* read HCR register */
	tst   r0, #1                /* check VM bit      */
	beq   _host_to_vm
	mov   r0, #\exception_type
	b     _vm_to_host
.endm /* _vm_exit */


.section .text

/*
 * On virtualization exceptions the CPU has to jump to one of the following
 * 7 entry vectors to switch to a kernel context.
 */
.p2align 12
.global hypervisor_exception_vector
hypervisor_exception_vector:
	b _vt_rst_entry
	b _vt_und_entry    /* undefined instruction  */
	b _vt_svc_entry    /* hypervisor call        */
	b _vt_pab_entry    /* prefetch abort         */
	b _vt_dab_entry    /* data abort             */
	b _vt_trp_entry    /* hypervisor trap        */
	b _vt_irq_entry    /* interrupt request      */
	_vm_exit 7         /* fast interrupt request */

_vt_rst_entry: _vm_exit 1
_vt_und_entry: _vm_exit 2
_vt_svc_entry: _vm_exit 3
_vt_pab_entry: _vm_exit 4
_vt_dab_entry: _vm_exit 5
_vt_irq_entry: _vm_exit 6
_vt_trp_entry: _vm_exit 8

_host_to_vm:
	push  { r1 }
	ldr   r0, [sp, #1*4]
	add   r0, r0, #13*4
	ldmia r0!, { r1-r5 }
	msr   sp_usr,    r1
	mov   lr,        r2
	msr   elr_hyp,   r3
	msr   spsr_cxfs, r4
	ldmia r0!, { r1-r12 }
	msr   spsr_und,  r1
	msr   sp_und,    r2
	msr   lr_und,    r3
	msr   spsr_svc,  r4
	msr   sp_svc,    r5
	msr   lr_svc,    r6
	msr   spsr_abt,  r7
	msr   sp_abt,    r8
	msr   lr_abt,    r9
	msr   spsr_irq,  r10
	msr   sp_irq,    r11
	msr   lr_irq,    r12
	ldmia r0!, {r5 - r12}
	msr   spsr_fiq,  r5
	msr   sp_fiq,    r6
	msr   lr_fiq,    r7
	msr   r8_fiq,    r8
	msr   r9_fiq,    r9
	msr   r10_fiq,   r10
	msr   r11_fiq,   r11
	msr   r12_fiq,   r12
	ldmia r0!, {r1 - r12}
	mcrr  p15, 6, r1, r2, c2      /* write VTTBR        */
	mcr   p15, 0, r3, c1, c0, 0   /* write SCTRL        */
	mcr   p15, 4, r4, c1, c1, 3   /* write HSTR         */
	mcr   p15, 4, r5, c1, c1, 0   /* write HCR register */
	mcr   p15, 0, r8, c2, c0, 2   /* write TTBRC        */
	mcr   p15, 0, r9, c2, c0, 0   /* write TTBR0        */
	mcr   p15, 0, r10, c2, c0, 1  /* write TTBR1        */
	mcr   p15, 0, r11, c10, c2, 0 /* write PRRR         */
	mcr   p15, 0, r12, c10, c2, 1 /* write NMRR         */
	ldmia r0!, {r1-r12}
	mcr   p15, 0, r1, c3, c0, 0   /* write DACR         */
	mcr   p15, 0, r2, c5, c0, 0   /* write DFSR         */
	mcr   p15, 0, r3, c5, c0, 1   /* write IFSR         */
	mcr   p15, 0, r4, c5, c1, 0   /* write ADFSR        */
	mcr   p15, 0, r5, c5, c1, 1   /* write AIFSR        */
	mcr   p15, 0, r6, c6, c0, 0   /* write DFAR         */
	mcr   p15, 0, r7, c6, c0, 2   /* write IFAR         */
	mcr   p15, 0, r8, c13, c0, 1  /* write CIDR         */
	mcr   p15, 0, r9, c13, c0, 2  /* write TLS1         */
	mcr   p15, 0, r10, c13, c0, 3 /* write TLS2         */
	mcr   p15, 0, r11, c13, c0, 4 /* write TLS3         */
	mcr   p15, 0, r12, c1,  c0, 2 /* write CPACR        */
	ldmia r0!, {r1-r2}
	mcr   p15, 4, r1,  c0,  c0, 5 /* write VMPIDR       */
	vmsr  fpscr, r2
	vldm  r0!, {d0-d15}
	vldm  r0!, {d16-d31}
	ldmia r0!, {r1-r6}
	mcrr  p15, 4, r1, r2,  c14    /* write cntvoff      */
	mcrr  p15, 3, r3, r4,  c14    /* write cntv_cval    */
	mcr   p15, 0, r5, c14, c3, 1  /* write cntv_ctl     */
	mcr   p15, 0, r6, c14, c1, 0  /* write cntkctl      */
	ldr   r0, [sp, #1*4]
	ldmia r0, {r0-r12}            /* load vm's r0-r12   */
	eret

_vm_to_host:
	push  { r0 }                  /* push cpu excep.    */
	ldr   r0,  [sp, #3*4]         /* load vm state ptr  */
	add   r0,  r0,  #1*4          /* skip r0            */
	stmia r0!, {r1-r12}           /* save regs r1-r12   */
	pop   { r5 }                  /* pop cpu excep.     */
	pop   { r1 }                  /* pop r0             */
	str   r1,  [r0, #-13*4]       /* save r0            */
	mrs   r1,  sp_usr             /* read USR sp        */
	mov   r2,  lr                 /* read USR lr        */
	mrs   r3,  elr_hyp            /* read ip            */
	mrs   r4,  spsr               /* read cpsr          */
	mrs   r6,  spsr_und
	mrs   r7,  sp_und
	mrs   r8,  lr_und
	mrs   r9,  spsr_svc
	mrs   r10, sp_svc
	mrs   r11, lr_svc
	mrs   r12, spsr_abt
	stmia r0!, {r1-r12}
	mrs   r1,  sp_abt
	mrs   r2,  lr_abt
	mrs   r3,  spsr_irq
	mrs   r4,  sp_irq
	mrs   r5,  lr_irq
	mrs   r6,  spsr_fiq
	mrs   r7,  sp_fiq
	mrs   r8,  lr_fiq
	mrs   r9,  r8_fiq
	mrs   r10, r9_fiq
	mrs   r11, r10_fiq
	mrs   r12, r11_fiq
	stmia r0!, {r1-r12}
	mrs   r1,  r12_fiq
	str   r1,  [r0]
	add   r0,  r0, #3*4           /* skip VTTBR             */
	mrc   p15, 0, r1,  c1,  c0, 0 /* read SCTRL             */
	mrc   p15, 4, r2,  c5,  c2, 0 /* read HSR               */
	mrc   p15, 4, r3,  c6,  c0, 4 /* read HPFAR             */
	mrc   p15, 4, r4,  c6,  c0, 0 /* read HDFAR             */
	mrc   p15, 4, r5,  c6,  c0, 2 /* read HIFAR             */
	mrc   p15, 0, r6,  c2,  c0, 2 /* read TTBRC             */
	mrc   p15, 0, r7,  c2,  c0, 0 /* read TTBR0             */
	mrc   p15, 0, r8,  c2,  c0, 1 /* read TTBR1             */
	mrc   p15, 0, r9,  c10, c2, 0 /* read PRRR              */
	mrc   p15, 0, r10, c10, c2, 1 /* read NMRR              */
	mrc   p15, 0, r11, c3,  c0, 0 /* read DACR              */
	mrc   p15, 0, r12, c5,  c0, 0 /* read DFSR              */
	stmia r0!, {r1-r12}
	mrc   p15, 0, r1,  c5,  c0, 1 /* read IFSR              */
	mrc   p15, 0, r2,  c5,  c1, 0 /* read ADFSR             */
	mrc   p15, 0, r3,  c5,  c1, 1 /* read AIFSR             */
	mrc   p15, 0, r4,  c6,  c0, 0 /* read DFAR              */
	mrc   p15, 0, r5,  c6,  c0, 2 /* read IFAR              */
	mrc   p15, 0, r6,  c13, c0, 1 /* read CIDR              */
	mrc   p15, 0, r7,  c13, c0, 2 /* read TLS1              */
	mrc   p15, 0, r8,  c13, c0, 3 /* read TLS2              */
	mrc   p15, 0, r9,  c13, c0, 4 /* read TLS3              */
	mrc   p15, 0, r10, c1,  c0, 2 /* read CPACR             */
	mrc   p15, 4, r11, c0,  c0, 5 /* read VMPIDR            */
	stmia r0!, {r1-r11}
	mov   r1,  #1                 /* clear fpu excep. state */
	lsl   r1,  #30
	vmsr  fpexc, r1
	vmrs  r12, fpscr
	stmia r0!, {r12}
	vstm  r0!, {d0-d15}
	vstm  r0!, {d16-d31}
	mrrc  p15, 4, r1,  r2,  c14   /* read cntvoff           */
	mrrc  p15, 3, r3,  r4,  c14   /* read cntv_cval         */
	mrc   p15, 0, r5,  c14, c3, 1 /* read cntv_ctl          */
	mrc   p15, 0, r6,  c14, c1, 0 /* read cntkctl           */
	stmia r0!, {r1-r6}

	clrex
	dsb sy
	isb sy

	/**************************
	 ** Restore host context **
	 **************************/

	pop   { r0, r1 }
	ldmia r0!, {r1-r12}
	mcrr  p15, 6, r1,  r2,  c2    /* write VTTBR            */
	mcr   p15, 4, r3,  c1,  c1, 0 /* write HCR register     */
	mcr   p15, 4, r4,  c1,  c1, 3 /* write HSTR register    */
	mcr   p15, 0, r5,  c1,  c0, 2 /* write CPACR            */
	msr   sp_svc,    r6
	msr   elr_hyp,   r7
	msr   spsr_cxfs, r8
	mcrr  p15, 0, r9,  r10, c2    /* write TTBR0            */
	mcrr  p15, 1, r11, r12, c2    /* write TTBR1            */
	ldmia r0,  {r1-r5}
	mcr   p15, 0, r1,  c1,  c0, 0 /* write SCTRL            */
	mcr   p15, 0, r2,  c2,  c0, 2 /* write TTBRC            */
	mcr   p15, 0, r3,  c10, c2, 0 /* write MAIR0            */
	mcr   p15, 0, r4,  c3,  c0, 0 /* write DACR             */
	mcr   p15, 4, r5,  c0,  c0, 5 /* write VMPIDR           */
	eret


/* host kernel must jump to this point to switch to a vm */
.global hypervisor_enter_vm
hypervisor_enter_vm:
	hvc   #0
