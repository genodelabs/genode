/*
 * \brief  Transition between kernel/userland
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

#include <synchronize.s>

/*********************
 ** Constant values **
 *********************/

.set USR_MODE, 16
.set FIQ_MODE, 17
.set IRQ_MODE, 18
.set SVC_MODE, 19
.set ABT_MODE, 23
.set UND_MODE, 27
.set SYS_MODE, 31

.set RST_TYPE, 1
.set UND_TYPE, 2
.set SVC_TYPE, 3
.set PAB_TYPE, 4
.set DAB_TYPE, 5
.set IRQ_TYPE, 6
.set FIQ_TYPE, 7

.set RST_PC_ADJUST, 0
.set UND_PC_ADJUST, 4
.set SVC_PC_ADJUST, 0
.set PAB_PC_ADJUST, 4
.set DAB_PC_ADJUST, 8
.set IRQ_PC_ADJUST, 4
.set FIQ_PC_ADJUST, 4

/* offsets of CPU context members */
.set PC_OFFSET,    15 * 4
.set STACK_OFFSET, 17 * 4


/************
 ** Macros **
 ************/

/**
 * Save an interrupted user context and switch to kernel context
 *
 * \param exception_type  kernel name of exception type
 * \param mode            mode number of program status register
 * \param pc_adjust       value that gets subtracted from saved user PC
 */
.macro _user_to_kernel exception_type, mode, pc_adjust

	cpsid f, #SVC_MODE            /* disable interrupts and change to SVC mode */
	sub   sp, sp, #0x150          /* make room for cpu context */
	stm   sp, {r0-r14}^           /* the sp_svc contains the user context pointer */
	add   r0, sp, #PC_OFFSET
	cps   #\mode
	sub   r1, lr, #\pc_adjust     /* calculate user program counter */
	mrs   r2, spsr                /* get user cpsr */
	mov   r3, #\exception_type
	b     _common_kernel_entry

.endm


.section .text.crt0

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

	/*
	 * Fast interrupt exception entry 0x1c.
	 *
	 * If the previous mode was not the user mode, it means a previous
	 * exception got interrupted by a fast interrupt.
	 * In that case, we disable fast interrupts and return to the
	 * previous exception handling routine.
	 */
	mrs  r8, spsr
	and  r9, r8, #0b11111
	cmp  r9, #USR_MODE
	cmpne r9, #SYS_MODE
	beq  _fiq_entry
	orr  r8, #0b1000000
	msr  spsr_cxsf, r8
	subs pc, lr, #4

	_rst_entry: _user_to_kernel RST_TYPE, SVC_MODE, RST_PC_ADJUST
	_und_entry: _user_to_kernel UND_TYPE, UND_MODE, UND_PC_ADJUST
	_svc_entry: _user_to_kernel SVC_TYPE, SVC_MODE, SVC_PC_ADJUST
	_pab_entry: _user_to_kernel PAB_TYPE, ABT_MODE, PAB_PC_ADJUST
	_dab_entry: _user_to_kernel DAB_TYPE, ABT_MODE, DAB_PC_ADJUST
	_irq_entry: _user_to_kernel IRQ_TYPE, IRQ_MODE, IRQ_PC_ADJUST
	_fiq_entry: _user_to_kernel FIQ_TYPE, FIQ_MODE, FIQ_PC_ADJUST

	_common_kernel_entry:
	stmia r0!, {r1-r3}       /* save pc, cpsr and exception type */
	clrex                    /* clear exclusive access needed for cmpxchg */
	cps   #SVC_MODE

	mov   r1, #1             /* clear exception state of the VFP */
	lsl   r1, #30
	vmsr  fpexc, r1
	adr   r1, _fpu_save
	ldr   r1, [r1]
	blx   r1

	SYSTEM_REGISTER_SYNC_BARRIER /* synchronize after the context switch */

	/*
	 * Go to kernel entry code
	 */
	adr   lr, _kernel_entry
	ldr   lr, [lr]
	mov   r0, sp
	bx    lr

	_kernel_entry:
	.long _ZN6Kernel24main_handle_kernel_entryEPN6Genode9Cpu_stateE

	_fpu_save:
	.long vfp_save_fpu_context


.section .text

	/*******************************
	 ** idle loop for idle thread **
	 *******************************/

	.global idle_thread_main
	idle_thread_main:
	wfi
	b idle_thread_main


	/*****************************
	 ** kernel to userland switch **
	 *******************************/

	.global kernel_to_user_context_switch
	kernel_to_user_context_switch:
	mov   sp, r2
	bl    vfp_load_fpu_context
	ldr   lr, [r1, #15*4]
	ldr   r0, [r1, #16*4]
	msr   spsr_cxsf, r0
	add   r0, r1, #4
	ldm   r0, {r1-r14}^
	sub   r0, r0, #4
	ldr   r0, [r0]
	SYSTEM_REGISTER_SYNC_BARRIER /* synchronize after the context switch */
	subs  pc, lr, #0
