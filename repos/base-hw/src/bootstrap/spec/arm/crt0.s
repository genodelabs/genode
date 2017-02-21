/*
 * \brief   Startup code for bootstrap
 * \author  Stefan Kalkowski
 * \date    2016-09-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.set STACK_SIZE, 4 * 16 * 1024

.section ".text.crt0"

	.global _start
	_start:


	/***************************
	 ** Zero-fill BSS segment **
	 ***************************/

	adr r0, _bss_local_start
	adr r1, _bss_local_end
	ldr r0, [r0]
	ldr r1, [r1]
	mov r2, #0
	1:
	cmp r1, r0
	ble 2f
	str r2, [r0]
	add r0, r0, #4
	b   1b
	2:


	/*****************************************************
	 ** Setup multiprocessor-aware kernel stack-pointer **
	 *****************************************************/

	mov sp, #0                  /* for boot cpu use id 0    */
	cps #31                     /* change to system mode    */

	.global _start_setup_stack  /* entrypoint for all cpus */
	_start_setup_stack:

	mrs r2, cpsr
	cmp r2, #31                 /* check for system mode */
	mrcne p15, 0, sp, c0, c0, 5 /* read multiprocessor affinity register */
	andne sp, sp, #0xff         /* set cpu id for non-boot cpu */
	cps #19                     /* change to supervisor mode */

	adr r0, _start_stack        /* load stack address into r0 */
	adr r1, _start_stack_size   /* load stack size per cpu into r1 */
	ldr r1, [r1]

	add sp, #1                  /* calculate stack start for CPU */
	mul r1, r1, sp
	add sp, r0, r1


	/************************************
	 ** Jump to high-level entry point **
	 ************************************/

	b init


	_bss_local_start:
	.long _bss_start

	_bss_local_end:
	.long _bss_end

	_start_stack_size:
	.long STACK_SIZE

	.align 3
	_start_stack:
	.rept NR_OF_CPUS
		.space STACK_SIZE
	.endr
