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

.include "macros.s"

.set STACK_SIZE, 4 * 16 * 1024

.section ".text.crt0"

	.global _start
	_start:

	/* zero-fill BSS segment */
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

	.global _start_setup_stack
	_start_setup_stack:

	/* setup multiprocessor-aware kernel stack-pointer */
	adr r0, _start_stack
	adr r1, _start_stack_size
	ldr r1, [r1]
	_init_kernel_sp r0, r1
	b    init

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
