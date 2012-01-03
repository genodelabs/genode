/*
 * \brief  Access to execution context internals
 * \author Martin Stein
 * \date   2010-10-06
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/**********************************************************
 ** Overwrite specific parts of the execution context    **
 **                                                      **
 ** \param  r15  base address of the execution context   **
 ** \param  (rx)  value that shall be written - except   **
 **               r1, rpc and r15 - the register itself, **
 **               otherwise use "prestore" labels or r3  **
 **********************************************************/

.macro _SAVE_R2_TO_R13_TO_CONTEXT__USES_R2_TO_R13_R15
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
.endm


.macro _SAVE_R18_TO_R31_TO_CONTEXT__TO_R15_R18_TO_R31
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
.endm


.macro _SAVE_PRESTORED_R1_R15_RPC_TO_CONTEXT__USES_R3_R15
	lwi r3, r0, _prestored_r1
	swi r3, r15,  1*4
	lwi r3, r0, _prestored_r15
	swi r3, r15,  15*4
	lwi r3, r0, _prestored_rpc
	swi r3, r15,  32*4
.endm


.macro _SAVE_RMSR_TO_CONTEXT__USES_R3_R15
	mfs r3, rmsr
	swi r3, r15, 33*4
.endm


.macro _SAVE_RPID_TO_CONTEXT__USES_R3_R15
	mfs r3, rpid
	swi r3, r15, 36*4
.endm


.macro _SAVE_BLOCKING_TYPE_TO_CONTEXT__USES_R3_R15
	swi r3, r15, 37*4
.endm


.macro _SAVE_RESR_TO_CONTEXT__USES_R3_R15
	mfs r3, resr
	swi r3, r15, 35*4
.endm


.macro _SAVE_REAR_TO_CONTEXT__USES_R3_R15
	mfs r3, rear
	swi r3, r15, 34*4
.endm



/***********************************************************
 ** Load a specifics values from the execution context    **
 **                                                       **
 ** \param  r15  base address of the execution context    **
 ** \return  (rx)  value that has been loaded - except    **
 **                r1, rpc and r15 - the register itself, **
 **                otherwise use "preload" labels or r3   **
 ***********************************************************/

.macro _LOAD_R2_TO_R13_FROM_CONTEXT__USES_R2_TO_R13_R15
	lwi r2,  r15,  2*4
	lwi r3,  r15,  3*4
	lwi r4,  r15,  4*4
	lwi r5,  r15,  5*4
	lwi r6,  r15,  6*4
	lwi r7,  r15,  7*4
	lwi r8,  r15,  8*4
	lwi r9,  r15,  9*4
	lwi r10, r15, 10*4
	lwi r11, r15, 11*4
	lwi r12, r15, 12*4
	lwi r13, r15, 13*4
.endm


.macro _LOAD_R18_TO_R31_FROM_CONTEXT__TO_R15_R18_TO_R31
	lwi r18, r15, 18*4
	lwi r19, r15, 19*4
	lwi r20, r15, 20*4
	lwi r21, r15, 21*4
	lwi r22, r15, 22*4
	lwi r23, r15, 23*4
	lwi r24, r15, 24*4
	lwi r25, r15, 25*4
	lwi r26, r15, 26*4
	lwi r27, r15, 27*4
	lwi r28, r15, 28*4
	lwi r29, r15, 29*4
	lwi r30, r15, 30*4
	lwi r31, r15, 31*4
.endm


.macro _PRELOAD_R1_R15_RPC_FROM_CONTEXT__USES_R3_R15
	lwi r3, r15,  1*4
	swi r3, r0, _preloaded_r1
	lwi r3, r15,  15*4
	swi r3, r0, _preloaded_r15
	lwi r3, r15,  32*4
	swi r3, r0, _preloaded_rpc
.endm


.macro _LOAD_RMSR_FROM_CONTEXT__USES_R3_R4_R15
	lwi r3, r15, 33*4
	mts rmsr, r3
	_SYNCHRONIZING_OP
.endm


.macro _LOAD_RPID_FROM_CONTEXT__USES_R3_R15
	lwi r3, r15, 36*4
	mts rpid, r3
	_SYNCHRONIZING_OP
.endm


.macro _LOAD_BLOCKING_TYPE_FROM_CONTEXT__USES_R3_R15
	lwi r3, r15, 37*4
.endm



.macro _VARIABLES_TO_WRITE_EXEC_CONTEXT
	.align 4
	_prestored_r1:  .space 4
	.align 4
	_prestored_r15: .space 4
	.align 4
	_prestored_rpc: .space 4
.endm


.macro _VARIABLES_TO_READ_EXEC_CONTEXT
	.align 4
	_preloaded_r1:  .space 4
	.align 4
	_preloaded_r15: .space 4
	.align 4
	_preloaded_rpc: .space 4
.endm




/**
 * Macros to print out context content
 *
 * (any value hexadecimal and padded with 
 * leading zeros to 8 digits)
 * output has following format:
 * 
 * r1 r2 r3 r4
 * ...
 * ...
 * r28 r29 r30 r31
 * rmsr
 * 
 */


.macro _4BIT_SHIFT_RIGHT__ARG_30__RET_30
	srl r30, r30
	srl r30, r30
	srl r30, r30
	srl r30, r30
.endm


.macro _PRINT_ASCII8__ARG_30
	swi r30, r0, 0x84000004
.endm


.macro _PRINT_HEX8__ARG_30
	swi r29, r0, _print_hex8__buffer_0

	andi r30, r30, 0xf
	rsubi r29, r30, 9
	addi r30, r30, 48

	bgei r29, 8
	addi r30, r30, 39
	_PRINT_ASCII8__ARG_30

	lwi r29, r0, _print_hex8__buffer_0
.endm


.macro _PRINT_HEX32__ARG_31
	swi r31, r0, _print_hex32__buffer_1
	swi r30, r0, _print_hex32__buffer_0

	lwi r31, r0, _print_hex32__buffer_1-3
	add r30, r31, r0
	_4BIT_SHIFT_RIGHT__ARG_30__RET_30
	_PRINT_HEX8__ARG_30
	add r30, r31, r0
	_PRINT_HEX8__ARG_30

	lwi r31, r0, _print_hex32__buffer_1-2
	add r30, r31, r0
	_4BIT_SHIFT_RIGHT__ARG_30__RET_30
	_PRINT_HEX8__ARG_30
	add r30, r31, r0
	_PRINT_HEX8__ARG_30

	lwi r31, r0, _print_hex32__buffer_1-1
	add r30, r31, r0
	_4BIT_SHIFT_RIGHT__ARG_30__RET_30
	_PRINT_HEX8__ARG_30
	add r30, r31, r0
	_PRINT_HEX8__ARG_30

	lwi r31, r0, _print_hex32__buffer_1-0
	add r30, r31, r0
	_4BIT_SHIFT_RIGHT__ARG_30__RET_30
	_PRINT_HEX8__ARG_30
	add r30, r31, r0
	_PRINT_HEX8__ARG_30

	lwi r31, r0, _print_hex32__buffer_1
	lwi r30, r0, _print_hex32__buffer_0
.endm


.macro _PRINT_ASCII_SPACE
	swi r30, r0, _print_ascii_space__buffer_0

	addi r30, r0, 32
	_PRINT_ASCII8__ARG_30

	lwi r30, r0, _print_ascii_space__buffer_0
.endm


.macro _PRINT_ASCII_BREAK
	swi r30, r0, _print_ascii_break__buffer_0

	addi r30, r0, 13
	_PRINT_ASCII8__ARG_30
	addi r30, r0, 10
	_PRINT_ASCII8__ARG_30

	lwi r30, r0, _print_ascii_break__buffer_0
.endm


.macro _PRINT_ASCII_STOP
	swi r30, r0, _print_ascii_stop__buffer_0

	addi r30, r0, 115
	_PRINT_ASCII8__ARG_30
	addi r30, r0, 116
	_PRINT_ASCII8__ARG_30
	addi r30, r0, 111
	_PRINT_ASCII8__ARG_30
	addi r30, r0, 112
	_PRINT_ASCII8__ARG_30

	_PRINT_ASCII_BREAK

	lwi r30, r0, _print_ascii_stop__buffer_0
.endm


.macro _PRINT_ASCII_RUN
	swi r30, r0, _print_ascii_run__buffer_0

	addi r30, r0, 114
	_PRINT_ASCII8__ARG_30
	addi r30, r0, 117
	_PRINT_ASCII8__ARG_30
	addi r30, r0, 110
	_PRINT_ASCII8__ARG_30

	_PRINT_ASCII_BREAK

	lwi r30, r0, _print_ascii_run__buffer_0
.endm


.macro _PRINT_CONTEXT
	swi r31, r0, _print_context__buffer_0

	_PRINT_ASCII_BREAK


	add r31, r0, r0
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r1
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r2
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r3
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r4
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r5
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r6
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r7
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_BREAK


	add r31, r0, r8
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r9
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r10
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r11
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r12
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r13
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r14
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r15
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_BREAK


	add r31, r0, r16
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r17
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r18
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r19
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r20
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r21
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r22
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r23
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_BREAK


	add r31, r0, r24
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r25
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r26
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r27
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r28
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r29
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	add r31, r0, r30
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	lwi r31, r0, _print_context__buffer_0
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_BREAK


	mfs r31, rmsr
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE

	lwi r31, r0, _current_context_label
	_PRINT_HEX32__ARG_31
	_PRINT_ASCII_SPACE


	lwi r31, r0, _print_context__buffer_0
.endm


.macro _PRINT_ASCII_STOP__VARIABLES
	.align 4
	_print_ascii_stop__buffer_0: .space 1*4
.endm


.macro _PRINT_ASCII_RUN__VARIABLES
	.align 4
	_print_ascii_run__buffer_0: .space 1*4
.endm


.macro _PRINT_ASCII_BREAK__VARIABLES
	.align 4
	_print_ascii_break__buffer_0: .space 1*4
.endm


.macro _PRINT_ASCII_SPACE__VARIABLES
	.align 4
	_print_ascii_space__buffer_0: .space 1*4
.endm


.macro _PRINT_HEX8__VARIABLES
	.align 4
	_print_hex8__buffer_0: .space 1*4
	.align 4
	_print_hex8__buffer_1: .space 1*4
.endm


.macro _PRINT_HEX32__VARIABLES
	.align 4
	_print_hex32__buffer_0: .space 1*4
	.align 4
	_print_hex32__buffer_1: .space 1*4
.endm


.macro _PRINT_CONTEXT__VARIABLES
	.align 4
	_print_context__buffer_0: .space 1*4
.endm



