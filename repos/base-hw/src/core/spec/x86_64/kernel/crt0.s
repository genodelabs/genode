/*
 * \brief   Startup code for kernel
 * \author  Adrian-Ken Rueegsegger
 * \author  Martin Stein
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \date    2015-02-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.include "macros.s"

.section ".text.crt0"

.global _start
_start:
	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	leaq _stack_high@GOTPCREL(%rip),%rax
	movq (%rax), %rsp

	movq __initial_bx@GOTPCREL(%rip),%rax
	movq %rbx, (%rax)

	/* kernel-initialization */
	call init_kernel

	/* catch erroneous return of the kernel initialization */
	1: jmp 1b


/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.bss

	/* stack of the temporary initial environment */
	.p2align 8
	.space 32 * 1024
	_stack_high:
	.globl __initial_bx
	__initial_bx:
	.space 8
