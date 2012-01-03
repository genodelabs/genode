/*
 * \brief  Userland entry
 * \author Martin Stein
 * \date   2010-10-05
 *
 * Enter the userland via '_userland_entry' with execution context denoted in
 * '_userland_context'
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

.global _userland_entry
.global _userland_context

.global _kernel_timer_ctrl
.global _kernel_timer_ctrl_start


.macro _START_KERNEL_TIMER
	swi r30, r0, _start_kernel_timer__buf_0
	swi r31, r0, _start_kernel_timer__buf_1

	lwi r30, r0, _kernel_timer_ctrl
	lwi r31, r0, _kernel_timer_ctrl_start
	swi r31, r30, 0

	lwi r30, r0, _start_kernel_timer__buf_0
	lwi r31, r0, _start_kernel_timer__buf_1
.endm


.macro _START_KERNEL_TIMER__VARIABLES
	.align 4
	_start_kernel_timer__buf_0: 
	.space 1*4 
	.align 4
	_start_kernel_timer__buf_1: 
	.space 1*4 
.endm


.macro _MAY_BE_VERBOSE_USERLAND_ENTRY
/*
	_PRINT_CONTEXT 
	_PRINT_ASCII_RUN
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


.macro _PREPARE_CONTEXT__USES_R2_TO_R31
	lwi r15, r0, _userland_context

	_PRELOAD_R1_R15_RPC_FROM_CONTEXT__USES_R3_R15

	_LOAD_RPID_FROM_CONTEXT__USES_R3_R15
	_LOAD_RMSR_FROM_CONTEXT__USES_R3_R4_R15

	_LOAD_R2_TO_R13_FROM_CONTEXT__USES_R2_TO_R13_R15
	_LOAD_R18_TO_R31_FROM_CONTEXT__TO_R15_R18_TO_R31
.endm


.macro _EXEC_PREPARED_CONTEXT_CASE_INTERRUPT
	lwi r14, r0, _preloaded_rpc
	lwi  r1, r0, _preloaded_r1
	lwi r15, r0, _preloaded_r15

	_MAY_BE_VERBOSE_USERLAND_ENTRY
	_START_KERNEL_TIMER

	rtid r14, 0
	or r0, r0, r0
.endm


.macro _EXEC_PREPARED_CONTEXT_CASE_EXCEPTION
	lwi r1, r0,  _preloaded_r1
	lwi r17, r0, _preloaded_rpc
	lwi r15, r0, _preloaded_r15

	_MAY_BE_VERBOSE_USERLAND_ENTRY
	_START_KERNEL_TIMER

	rted r17, 0
	or r0, r0, r0
.endm


.macro _EXEC_PREPARED_CONTEXT_CASE_SYSCALL
	lwi  r1, r0,  _preloaded_r1
	lwi  r15, r0, _preloaded_rpc

	_MAY_BE_VERBOSE_USERLAND_ENTRY
	_START_KERNEL_TIMER

	rtbd r15, 8
	or r0, r0, r0
.endm


.macro _EXEC_PREPARED_CONTEXT_CASE_INITIAL

	lwi r14, r0, _preloaded_rpc
	lwi  r1, r0, _preloaded_r1
	lwi r15, r0, _preloaded_r15

	_ENABLE_EXCEPTIONS
	_START_KERNEL_TIMER

	rtid r14, 0
	or r0, r0, r0
.endm


.macro _SWITCH_USERLAND_BLOCKING_TYPE__USES_R3_R15
	lwi r15, r0, _userland_context
	_LOAD_BLOCKING_TYPE_FROM_CONTEXT__USES_R3_R15
.endm


.macro _CASE_INITIAL__USES_R4_R3
	xori r4, r3, 0
	bnei r4, _end_case_initial
.endm


.macro _CASE_INTERRUPT__USES_R4_R3
	xori r4, r3, 1
	bnei r4, _end_case_interrupt
.endm


.macro _CASE_EXCEPTION__USES_R4_R3
	xori r4, r3, 2
	bnei r4, _end_case_exception
.endm


.macro _CASE_SYSCALL__USES_R4_R3
	xori r4, r3, 3
	bnei r4, _end_case_syscall
.endm


_BEGIN_READABLE_EXECUTABLE

	_userland_entry:
		_SWITCH_USERLAND_BLOCKING_TYPE__USES_R3_R15
			_CASE_INTERRUPT__USES_R4_R3

				_PREPARE_CONTEXT__USES_R2_TO_R31
				_EXEC_PREPARED_CONTEXT_CASE_INTERRUPT
				/* userland execution */

			_end_case_interrupt:
			_CASE_EXCEPTION__USES_R4_R3

				_PREPARE_CONTEXT__USES_R2_TO_R31
				_EXEC_PREPARED_CONTEXT_CASE_EXCEPTION
				/* userland execution */

			_end_case_exception:
			_CASE_SYSCALL__USES_R4_R3

				_PREPARE_CONTEXT__USES_R2_TO_R31
				_EXEC_PREPARED_CONTEXT_CASE_SYSCALL
				/* userland execution */

			_end_case_syscall:
			_CASE_INITIAL__USES_R4_R3

				_PREPARE_CONTEXT__USES_R2_TO_R31
				_EXEC_PREPARED_CONTEXT_CASE_INITIAL
				/* userland execution */

			_end_case_initial:
			_case_default:

				_ERROR_UNKNOWN_USERLAND_BLOCKING_TYPE
				/* system halted */

			_end_case_default:
		_end_switch_userland_blocking_type:
	_end_userland_entry:


_BEGIN_READABLE_WRITEABLE

	.align 4
	_kernel_timer_ctrl:       .space 1*4
	.align 4
	_kernel_timer_ctrl_start: .space 1*4
	_START_KERNEL_TIMER__VARIABLES

	_VARIABLES_TO_READ_EXEC_CONTEXT
	_MAY_BE_VERBOSE_VARIABLES

	.align 4
	_userland_context:    .space 1*4

