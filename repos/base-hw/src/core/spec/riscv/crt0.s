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

.include "memory_consts.s"

.section ".text"

	/***********************
	 ** kernel entry code **
	 ***********************/

	.global _start
	_start:

	li x29, HW_MM_CPU_LOCAL_MEMORY_AREA_START
	li x30, HW_MM_CPU_LOCAL_MEMORY_SLOT_STACK_OFFSET
	add x29, x29, x30
	li x30, HW_MM_KERNEL_STACK_SIZE
	add sp, x29, x30
	la x30, _ZN6Kernel39main_initialize_and_handle_kernel_entryEv

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
