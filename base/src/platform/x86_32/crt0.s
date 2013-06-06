/**
 * \brief   Startup code for Genode applications
 * \author  Christian Helmuth
 * \date    2009-08-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
	.text
	.global _start

_start:
	mov %esp, __initial_sp

	/* XXX Switch to our own stack.  */
	leal _stack_high, %esp

	/* Clear the base pointer so that stack backtraces will work.  */
	xor %ebp,%ebp

	/* Jump into init C code */
	call _main

	/* We should never get here since _main does not return */
1:	int  $3
	jmp  2f
	.ascii "_main() returned."
2:	jmp  1b

	.globl	__dso_handle
__dso_handle: .long 0

/*--- .eh_frame (exception frames) -----------------*/
/*
	.section .eh_frame,"aw"
	.global	__EH_FRAME_BEGIN__
__EH_FRAME_BEGIN__:
*/

/*--- .bss (non-initialized data) ------------------*/
	.bss
	.p2align 4
	.global	_stack_low
_stack_low:
	.space	64*1024
	.global	_stack_high
_stack_high:

	/* initial value of the ESP register */
	.globl	__initial_sp
__initial_sp: .space 4
