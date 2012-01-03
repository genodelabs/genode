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


.include "special_registers.s"
.include "errors.s"
.include "exec_context.s"
.include "linker_commands.s"


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
	_SAVE_BLOCKING_TYPE_TO_CONTEXT__USES_R3_R15
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
	_SAVE_BLOCKING_TYPE_TO_CONTEXT__USES_R3_R15
.endm


.macro _PRESTORE_CONTEXT_AT_SYSCALL__USES_R1_R15
	swi   r1, r0, _prestored_r1
	swi   r0, r0, _prestored_r15
	swi  r15, r0, _prestored_rpc
.endm


.macro _BLOCKING_TYPE_IS_SYSCALL__USES_R3_R15
	addi r3, r0, 3
	lwi r15, r0, _userland_context
	_SAVE_BLOCKING_TYPE_TO_CONTEXT__USES_R3_R15
.endm


.macro _BACKUP_PRESTORED_CONTEXT__USES_R2_TO_R31
	lwi r15, r0, _userland_context

	_SAVE_R2_TO_R13_TO_CONTEXT__USES_R2_TO_R13_R15
	_SAVE_R18_TO_R31_TO_CONTEXT__TO_R15_R18_TO_R31

	_SAVE_RPID_TO_CONTEXT__USES_R3_R15
	_SAVE_RMSR_TO_CONTEXT__USES_R3_R15
	_SAVE_RESR_TO_CONTEXT__USES_R3_R15
	_SAVE_REAR_TO_CONTEXT__USES_R3_R15

	_SAVE_PRESTORED_R1_R15_RPC_TO_CONTEXT__USES_R3_R15
.endm


.macro _CALL_KERNEL__USES_R1_R15
	addi r1, r0, _kernel_stack_top
	addi r15, r0, _exit_kernel-2*4

	brai _kernel
	or r0, r0, r0
.endm




_BEGIN_READABLE_EXECUTABLE

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




_BEGIN_READABLE_WRITEABLE

	.global _current_context_label
	.align 4
	_current_context_label: .space 1*4

	_VARIABLES_TO_WRITE_EXEC_CONTEXT
	_MAY_BE_VERBOSE_VARIABLES


