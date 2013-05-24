/*
 * \brief   Startup code for ld.lib.so (linux_arm)
 * \author  Christian Prochaska
 * \date    2012-07-06
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
.section ".text.crt0"

	.globl _start_ldso
_start_ldso:

	ldr	sl, .L_GOT
.L_GOT_OFF:
	add	sl, pc, sl

	ldr r1, .initial_sp
	ldr r1, [sl, r1]
	str sp, [r1]

	/*
	 * environ = &argv[argc + 1]
	 * in Genode argc is always 1
	 */
	add sp, sp,#12
	ldr r1, .lx_environ
	ldr r1, [sl, r1]
	str sp, [r1]

	/* XXX Switch to our own stack.  */
	ldr r1, .stack_high
	ldr sp, [sl, r1]

	/* relocate ldso */
	mov r1, #0
	bl init_rtld

	/*
	 * Clear the frame pointer and the link register so that stack
	 * backtraces will work.
	 */
	mov fp, #0
	mov lr, #0

	/* Jump into init C code */
	b _main

	.L_GOT:		.word _GLOBAL_OFFSET_TABLE_ - (.L_GOT_OFF + 8)
	.initial_sp:    .word __initial_sp(GOT)
	.lx_environ:	.word lx_environ(GOT)
	.stack_high:	.word _stack_high(GOT)
