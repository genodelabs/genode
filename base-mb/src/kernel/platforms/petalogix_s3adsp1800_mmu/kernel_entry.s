/**
 * Enter the kernel through interrupt, exception or syscall
 * saves userland context state to execution context denoted at _userland_context
 *
 * \author  Martin Stein
 * \date  2010-10-06
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* We have to know wich userland context was the last that was executed */
.extern _userland_context

/* We have to use the initial stack pointer of kernel to
 * provide an execution context for every kernel entrance
*/
.extern _kernel_stack_top

/* Pointer to kernels handler routine for userland blockings */
.extern _kernel
.extern _kernel_exit

/* We have to use these labels when initializing cpu's entries */
.global _syscall_entry
.global _exception_entry
.global _interrupt_entry



.macro _MAY_BE_VERBOSE_KERNEL_ENTRY
/*
	_PRINT_CONTEXT 
	_PRINT_ASCII_STOP
*/
.endm


.macro _MAY_BE_VERBOSE_VARIABLES
/*
	_PRINT_CONTEXT__VARIABLES
	_PRINT_HEX32__VARIABLES
	_PRINT_HEX8__VARIABLES
	_PRINT_ASCII_SPACE__VARIABLES
	_PRINT_ASCII_BREAK__VARIABLES
	_PRINT_ASCII_RUN__VARIABLES
	_PRINT_ASCII_STOP__VARIABLES
*/
.endm


.macro _IF_BRANCH_WAS_BLOCKING_CAUSE__USES_R15
	mfs r15, resr
	andi r15, r15, 0x00001000
	beqi r15, _end_if_branch_was_blocking_cause
.endm


.macro _SET_PRESTORED_RPC_TO_BRANCH_TARGET__USES_R15
		mfs r15, rbtr
		swi r15, r0, _prestored_rpc
.endm


.macro _PRESTORE_CONTEXT_AT_INTERRUPT__USES_R1_R14_R15
	swi r14, r0, _prestored_rpc
	swi  r1, r0, _prestored_r1
	swi r15, r0, _prestored_r15
.endm


.macro _BLOCKING_TYPE_IS_INTERRUPT__USES_R3_R15
	addi r3, r0, 1
	lwi r15, r0, _userland_context

	/* _SAVE_BLOCKING_TYPE_TO_CONTEXT__USES_R3_R15 */
	swi r3, r15, 37*4
.endm


.macro _PRESTORE_CONTEXT_AT_EXCEPTION__USES_R1_R15_R17
	swi r17, r0, _prestored_rpc
	swi  r1, r0, _prestored_r1
	swi r15, r0, _prestored_r15

	_IF_BRANCH_WAS_BLOCKING_CAUSE__USES_R15

		_SET_PRESTORED_RPC_TO_BRANCH_TARGET__USES_R15

	_end_if_branch_was_blocking_cause:
.endm


.macro _BLOCKING_TYPE_IS_EXCEPTION__USES_R3_R15
	addi r3, r0, 2
	lwi r15, r0, _userland_context

	/* _SAVE_BLOCKING_TYPE_TO_CONTEXT__USES_R3_R15 */
	swi r3, r15, 37*4
.endm


.macro _PRESTORE_CONTEXT_AT_SYSCALL__USES_R1_R15
	swi   r1, r0, _prestored_r1
	swi   r0, r0, _prestored_r15
	swi  r15, r0, _prestored_rpc
.endm


.macro _BLOCKING_TYPE_IS_SYSCALL__USES_R3_R15
	addi r3, r0, 3
	lwi r15, r0, _userland_context

	/* _SAVE_BLOCKING_TYPE_TO_CONTEXT__USES_R3_R15 */
	swi r3, r15, 37*4
.endm


.macro _BACKUP_PRESTORED_CONTEXT__USES_R2_TO_R31
	lwi r15, r0, _userland_context

	/* _SAVE_R2_TO_R13_TO_CONTEXT__USES_R2_TO_R13_R15 */
	swi r2,  r15,  2*4
	swi r3,  r15,  3*4
	swi r4,  r15,  4*4
	swi r5,  r15,  5*4
	swi r6,  r15,  6*4
	swi r7,  r15,  7*4
	swi r8,  r15,  8*4
	swi r9,  r15,  9*4
	swi r10, r15, 10*4
	swi r11, r15, 11*4
	swi r12, r15, 12*4
	swi r13, r15, 13*4

	/* _SAVE_R18_TO_R31_TO_CONTEXT__TO_R15_R18_TO_R31 */
	swi r18, r15, 18*4
	swi r19, r15, 19*4
	swi r20, r15, 20*4
	swi r21, r15, 21*4
	swi r22, r15, 22*4
	swi r23, r15, 23*4
	swi r24, r15, 24*4
	swi r25, r15, 25*4
	swi r26, r15, 26*4
	swi r27, r15, 27*4
	swi r28, r15, 28*4
	swi r29, r15, 29*4
	swi r30, r15, 30*4
	swi r31, r15, 31*4

	/* _SAVE_RPID_TO_CONTEXT__USES_R3_R15 */
	mfs r3, rpid
	swi r3, r15, 36*4

	/* _SAVE_RMSR_TO_CONTEXT__USES_R3_R15 */
	mfs r3, rmsr
	swi r3, r15, 33*4

	/* _SAVE_RESR_TO_CONTEXT__USES_R3_R15 */
	mfs r3, resr
	swi r3, r15, 35*4

	/* _SAVE_REAR_TO_CONTEXT__USES_R3_R15 */
	mfs r3, rear
	swi r3, r15, 34*4

	/* _SAVE_PRESTORED_R1_R15_RPC_TO_CONTEXT__USES_R3_R15 */
	lwi r3, r0, _prestored_r1
	swi r3, r15,  1*4
	lwi r3, r0, _prestored_r15
	swi r3, r15,  15*4
	lwi r3, r0, _prestored_rpc
	swi r3, r15,  32*4
.endm


.macro _CALL_KERNEL__USES_R1_R15
	addi r1, r0, _kernel_stack_top
	addi r15, r0, _exit_kernel-2*4

	brai _kernel
	or r0, r0, r0
.endm


/* _BEGIN_READABLE_EXECUTABLE */
.section ".text"

	_interrupt_entry:

		_MAY_BE_VERBOSE_KERNEL_ENTRY

		_PRESTORE_CONTEXT_AT_INTERRUPT__USES_R1_R14_R15
		_BACKUP_PRESTORED_CONTEXT__USES_R2_TO_R31
		_BLOCKING_TYPE_IS_INTERRUPT__USES_R3_R15

		_CALL_KERNEL__USES_R1_R15
		/* Kernel execution with return pointer set to _kernel_exit */

	_end_interrupt_entry:




	_exception_entry:

		_MAY_BE_VERBOSE_KERNEL_ENTRY

		_PRESTORE_CONTEXT_AT_EXCEPTION__USES_R1_R15_R17
		_BACKUP_PRESTORED_CONTEXT__USES_R2_TO_R31
		_BLOCKING_TYPE_IS_EXCEPTION__USES_R3_R15

		_CALL_KERNEL__USES_R1_R15
		/* Kernel execution with return pointer set to _kernel_exit */

	_end_exception_entry:




	_syscall_entry:

		_MAY_BE_VERBOSE_KERNEL_ENTRY

		_PRESTORE_CONTEXT_AT_SYSCALL__USES_R1_R15
		_BACKUP_PRESTORED_CONTEXT__USES_R2_TO_R31
		_BLOCKING_TYPE_IS_SYSCALL__USES_R3_R15

		_CALL_KERNEL__USES_R1_R15
		/* Kernel execution with return pointer set to _kernel_exit */

	_end_syscall_entry:




/* _BEGIN_READABLE_WRITEABLE */
.section ".bss"

	.global _current_context_label
	.align 4
	_current_context_label: .space 1*4

	/* _VARIABLES_TO_WRITE_EXEC_CONTEXT */
	.align 4
	_prestored_r1:  .space 4
	.align 4
	_prestored_r15: .space 4
	.align 4
	_prestored_rpc: .space 4

	_MAY_BE_VERBOSE_VARIABLES


