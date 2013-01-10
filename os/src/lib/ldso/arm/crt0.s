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

	.globl _start_ldso
_start_ldso:

	ldr r2, .initial_utcb
	str r0, [r2]

	ldr  sp, .initial_sp
	bl init_rtld
	b    _main

	.initial_sp:   .word _stack_high
	.initial_utcb: .word _main_utcb

