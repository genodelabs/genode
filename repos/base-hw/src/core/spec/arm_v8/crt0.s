/**
 * \brief   Startup code for core on ARM
 * \author  Stefan Kalkowski
 * \date    2015-03-06
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

	/* switch to cpu-specific kernel stack */
	/*adr   r1, _kernel_stack
	adr   r2, _kernel_stack_size
	ldr   r1, [r1]
	ldr   r2, [r2]
	ldr   r2, [r2]
	add   r0, #1
	mul   r0, r0, r2
	add   sp, r1, r0*/

	/* jump into init C code */
	b kernel_init

	_kernel_stack:      .quad kernel_stack
	_kernel_stack_size: .quad kernel_stack_size


	/*********************************
	 ** core main thread entry code **
	 *********************************/

	.global _core_start
	_core_start:

	/* create proper environment for main thread */
	bl init_main_thread

	/* apply environment that was created by init_main_thread */
	ldr x0, =init_main_thread_result
	ldr x0, [x0]
	mov sp, x0

	/* jump into init C code instead of calling it as it should never return */
	b _main
