/**
 * \brief   Startup code for ld.lib.so (x86-32)
 * \author  Christian Helmuth
 * \author  Sebastian Sumpf
 * \author  Martin Stein
 * \date    2011-05-03
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/**************************
 ** .text (program code) **
 **************************/

.text

	/* linker entry-point */
	.global _start_ldso
	_start_ldso:

	/*
	 * Initialize GOT pointer in EBX.
	 *
	 * The follwing statement causes a text relocation which will be ignored
	 * by ldso itself, this is necessary since we don't have a valid stack
	 * pointer at this moment so a 'call' in order to retrieve our IP and thus
	 * calculate the GOT-position in the traditional manner is not possible on
	 * x86.
	 */
	3:
	movl $., %ebx
	addl $_GLOBAL_OFFSET_TABLE_ + (. - 3b) , %ebx

	/* make initial value of some registers available to higher-level code */
	movl %esp, __initial_sp@GOTOFF(%ebx)
	movl %eax, __initial_ax@GOTOFF(%ebx)
	movl %edi, __initial_di@GOTOFF(%ebx)

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	leal _stack_high@GOTOFF(%ebx), %esp

	/* let init_rtld relocate LDSO */
	call init_rtld

	/* create proper environment for the main thread */
	call init_main_thread

	/* apply environment that was created by init_main_thread */
	movl init_main_thread_result, %esp

	/* clear the base pointer so that stack backtraces will work */
	xor %ebp,%ebp

	/* jump into init C code */
	call _main

	/* we should never get here since _main does not return */
	1:
	int  $3
	jmp  2f
	.ascii "_main() returned."
	2:
	jmp  1b
