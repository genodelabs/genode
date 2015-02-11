/*
 * \brief   Startup code for core
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2011-10-01
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

.section ".text.crt0"

	/* magic multi-boot header to make GRUB happy */
	.long 0x1badb002
	.long 0x0
	.long 0xe4524ffe

	/**********************************
	 ** Startup code for primary CPU **
	 **********************************/

.code32
	.global _start
	_start:

	/* Enable PAE (prerequisite for IA-32e mode) */
	movl %cr4, %eax
	btsl $5, %eax
	movl %eax, %cr4

	/*
	 * Install initial temporary environment that is replaced later by the
	 * environment that init_main_thread creates.
	 */
	leaq _stack_high@GOTPCREL(%rip),%rax
	movq (%rax), %rsp

	/* uniprocessor kernel-initialization which activates multiprocessor */
	call init_kernel_up

	/*********************************************
	 ** Startup code that is common to all CPUs **
	 *********************************************/

	.global _start_secondary_cpus
	_start_secondary_cpus:

	/* do multiprocessor kernel-initialization */
	call init_kernel_mp

	/* call the kernel main-routine */
	call kernel

	/* catch erroneous return of the kernel main-routine */
	1: jmp 1b


/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.bss

	/* stack of the temporary initial environment */
	.p2align 8
	.space 32 * 1024
	_stack_high:
