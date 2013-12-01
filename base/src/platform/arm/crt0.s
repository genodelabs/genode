/**
 * \brief   Startup code for Genode applications on ARM
 * \author  Norman Feske
 * \date    2007-04-28
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
.section ".text.crt0"

	.globl _start
_start:

	ldr r4, .initial_sp
	str sp, [r4]

	ldr  sp, .stack_high
	b    _main

.initial_sp: .word __initial_sp
.stack_high: .word _stack_high

	.globl	__dso_handle
__dso_handle: .long 0

/*--- .bss (non-initialized data) ------------------*/
.section ".bss"

	.p2align 4
	.globl	_stack_low
_stack_low:
	.space	128*1024
	.globl	_stack_high
_stack_high:

	/* initial value of the SP register */
	.globl  __initial_sp
__initial_sp: .space 4
