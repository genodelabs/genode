/*
 * \brief   Startup code for Genode applications
 * \author  Christian Helmuth
 * \author  Christian Prochaska
 * \date    2006-07-06
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
	.text

	.globl _start
_start:

	ldr r1,=__initial_sp
	str sp,[r1]

	/*
	 * environ = &argv[argc + 1]
	 * in Genode argc is always 1
	 */
	add sp,sp,#12
	ldr r1,=lx_environ
	str sp,[r1]

	/* XXX Switch to our own stack.  */
	ldr sp,=_stack_high

	/* Clear the frame pointer and the link register so that stack backtraces will work. */
	mov fp,#0
	mov lr,#0

	/* Jump into init C code */
	b _main

/*--------------------------------------------------*/
	.data
	.globl	__dso_handle
__dso_handle:
	.long	0

	.globl	__initial_sp
__initial_sp:
	.long	0

/*--- .eh_frame (exception frames) -----------------*/
/*
	.section .eh_frame,"aw"
	.globl	__EH_FRAME_BEGIN__
__EH_FRAME_BEGIN__:
*/

/*--- .bss (non-initialized data) ------------------*/
	.bss
	.p2align 4
	.globl	_stack_low
_stack_low:
	.space	64*1024
	.globl	_stack_high
_stack_high:
