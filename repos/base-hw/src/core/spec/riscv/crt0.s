/**
 * \brief  Startup code for the core's userland part
 * \author Sebastian Sumpf
 * \date   2015-09-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.section ".text"

	/***********************
	 ** kernel entry code **
	 ***********************/

	.global _start
	_start:

	la x29, kernel_stack
	la x30, kernel_stack_size
	ld x30, (x30)
	add sp, x29, x30
	la x30, kernel_init

	jalr x30


	/*********************************
	 ** core main thread entry code **
	 *********************************/

	.global _core_start
	_core_start:

	/* create environment for main thread */
	jal init_main_thread

	/* load stack pointer from init_main_thread_result */
	la sp, init_main_thread_result
	ld sp, (sp)

	/* jump into init C-code */
	j _main
