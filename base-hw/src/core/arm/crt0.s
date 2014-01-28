/*
 * \brief   Startup code for core on ARM
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2011-10-01
 *
 * The code in this file is compliant to the general ARM instruction- and
 * register-set but the semantics are tested only on ARMv6 and ARMv7 by now.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/**
 * Do no operation for 'count' cycles
 */
.macro _nop count
	.rept \count
		mov r8, r8
	.endr
.endm

.section ".text.crt0"

	/* ELF entry symbol */
	.global _start
	_start:

		/* idle a little initially because 'u-boot' likes it this way */
		_nop 8

		/* zero-fill BSS segment, BSS boundaries must be aligned to 4 */
		.extern _bss_start
		.extern _bss_end
		ldr r0, =_bss_start
		ldr r1, =_bss_end
		mov r2, #0
		1:
		cmp r1, r0
		ble 2f
		str r2, [r0]
		add r0, r0, #4
		b 1b
		2:

		/* enable C++ to prepare the first kernel run */
		ldr sp, =_kernel_stack_high
		bl init_phys_kernel

		/* call kernel routine */
		.extern kernel
		_start_kernel:
		ldr sp, =_kernel_stack_high
		bl kernel

		/* catch erroneous kernel return */
		3: b 3b

.section .bss

	/* kernel stack */
	.align 3
	.space 64*1024
	.global _kernel_stack_high
	_kernel_stack_high:

	/* main-thread UTCB-pointer for the Genode thread-API */
	.align 3
	.global _main_thread_utcb
	_main_thread_utcb: .long 0

