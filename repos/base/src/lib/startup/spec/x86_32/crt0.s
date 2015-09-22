/**
 * \brief   Startup code for Genode applications
 * \author  Christian Helmuth
 * \author  Martin Stein
 * \date    2009-08-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/**************************
 ** .text (program code) **
 **************************/

.section ".text.crt0"

	/* program entry-point */
	.global _start
	_start:

	/* save initial register values using GOT-relative addressing */
	3:
	movl $., %ecx
	addl $_GLOBAL_OFFSET_TABLE_ + (. - 3b) , %ecx
	movl %esp, __initial_sp@GOTOFF(%ecx)
	movl %eax, __initial_ax@GOTOFF(%ecx)
	movl %ebx, __initial_bx@GOTOFF(%ecx)
	movl %edi, __initial_di@GOTOFF(%ecx)

	/* initialize GOT pointer in EBX as expected by the tool chain */
	mov %ecx, %ebx

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	leal _stack_high@GOTOFF(%ebx), %esp

	/* if this is the dynamic linker, init_rtld relocates the linker */
	call init_rtld

	/* create proper environment for the main thread */
	call init_main_thread

	/* apply environment that was created by init_main_thread */
	movl init_main_thread_result, %esp

	/* clear the base pointer in order that stack backtraces will work */
	xor %ebp, %ebp

	/* jump into init C code instead of calling it as it should never return */
	jmp _main


/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.bss

	/* stack of the temporary initial environment */
	.p2align 4
	.space 32 * 1024
	_stack_high:

	/* initial value of the ESP, EAX and EDI register */
	.global __initial_sp
	__initial_sp:
	.space 4
	.global __initial_ax
	__initial_ax:
	.space 4
	.global __initial_bx
	__initial_bx:
	.space 4
	.global __initial_di
	__initial_di:
	.space 4
