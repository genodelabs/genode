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

.include "memory_consts.s"

.section ".text"

	/***********************
	 ** kernel entry code **
	 ***********************/

	.global _start
	_start:

	/* switch to cpu-specific kernel stack */
	adr   r1, _cpu_mem_area_base
	adr   r2, _cpu_mem_slot_size
	adr   r3, _kernel_stack_size
	ldr   r1, [r1]
	ldr   r2, [r2]
	ldr   r3, [r3]
	mul   r0, r0, r2
	add   r0, r0, r1
	add   sp, r0, r3

	/* jump into init C code */
	b _ZN6Kernel39main_initialize_and_handle_kernel_entryEv

	_cpu_mem_area_base: .long HW_MM_CPU_LOCAL_MEMORY_AREA_START
	_cpu_mem_slot_size: .long HW_MM_CPU_LOCAL_MEMORY_SLOT_SIZE
	_kernel_stack_size: .long HW_MM_KERNEL_STACK_SIZE


	/*********************************
	 ** core main thread entry code **
	 *********************************/

	.global _core_start
	_core_start:

	/* create proper environment for main thread */
	bl init_main_thread

	/* apply environment that was created by init_main_thread */
	ldr sp, =init_main_thread_result
	ldr sp, [sp]

	/* jump into init C code instead of calling it as it should never return */
	b _main
