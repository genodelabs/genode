/*
 * \brief   Startup code for ldso 64Bit version
 * \author  Christian Helmuth
 * \author  Sebastian Sumpf
 * \author  Martin Stein
 * \date    2011-05-10
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
	.globl _start_ldso
	_start_ldso:

	/* initialize global offset table */
	leaq _GLOBAL_OFFSET_TABLE_(%rip),%r15

	/* make initial value of some registers available to higher-level code */
	movq __initial_ax@GOTPCREL(%rip), %rbx
	movq %rax, (%rbx)
	movq __initial_di@GOTPCREL(%rip), %rbx
	movq %rdi, (%rbx)
	movq __initial_sp@GOTPCREL(%rip), %rax
	movq %rsp, (%rax)

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	leaq _stack_high@GOTPCREL(%rip),%rax
	movq (%rax), %rsp

	/* let init_rtld relocate LDSO */
	call init_rtld

	/* create proper environment for the main thread */
	call init_main_thread

	/* apply environment that was created by init_main_thread */
	movq init_main_thread_result@GOTPCREL(%rip), %rax
	movq (%rax), %rsp

	/* clear the base pointer so that stack backtraces will work */
	xorq %rbp,%rbp

	/* jump into init C code */
	call _main

	/* we should never get here since _main does not return */
	1:
	int  $3
	jmp  2f
	.ascii "_main() returned."
	2:
	jmp  1b
