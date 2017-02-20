/**
 * \brief   Startup code for Genode applications
 * \author  Sebastian Sumpf
 * \date    2016-02-16
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.section ".text"

.global _start
_start:

	/* load stack pointer */
	lla sp, _stack_high

	/* relocate linker */
	jal init_rtld

	/* create environment for main thread */
	jal init_main_thread

	/* load stack pointer from init_main_thread_result */
	la sp, init_main_thread_result
	ld sp, (sp)

	mv s0, x0

	/* jump into init C-code */
	j _main

.bss
	.p2align 8
	.global __initial_stack_base
	__initial_stack_base:
	.space 4*1024
	_stack_high:
