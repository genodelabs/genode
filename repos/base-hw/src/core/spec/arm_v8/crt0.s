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

/**
 * Store CPU number in register x0
 */
.macro _cpu_number
	mrs x0, mpidr_el1
	and x8, x0, #(1<<24) /* MT bit */
	cbz x8, 1f
	lsr x0, x0, #8
1:
	and x0, x0, #0b11111111
.endm

.section ".text"

	/***********************
	 ** kernel entry code **
	 ***********************/

	.global _start
	_start:

	/* switch to cpu-specific kernel stack */
	_cpu_number
	ldr x1, =_cpus_base
	ldr x1, [x1]
	mov x2, #HW_MM_CPU_LOCAL_MEMORY_SLOT_SIZE
	mul x0, x0, x2
	add x1, x1, x0
	mov x2, #HW_MM_KERNEL_STACK_SIZE
	add x1, x1, x2
	mov x2, #0x10 /* minus 16-byte to be aligned in stack */
	sub x1, x1, x2
	mov sp, x1

	/* jump into init C code */
	b _ZN6Kernel39main_initialize_and_handle_kernel_entryEv

	_cpus_base: .quad HW_MM_CPU_LOCAL_MEMORY_AREA_START


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
