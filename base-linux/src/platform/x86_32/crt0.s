/*
 * \brief   Startup code for Genode applications
 * \author  Christian Helmuth
 * \date    2006-07-06
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
	.text

	.globl _start
_start:

	movl %esp, __initial_sp
	/*
	 * environ = &argv[argc + 1]
	 * in Genode argc is always 1
	 */
	popl %eax /* argc */
	popl %eax /* argv[0] */
	popl %eax /* NULL */
	movl %esp, lx_environ

	/* XXX Switch to our own stack.  */
	movl $_stack_high,%esp

	/* Clear the base pointer so that stack backtraces will work.  */
	xorl %ebp,%ebp

	/* Jump into init C code */
	call _main

	/* We should never get here since _main does not return */
1:	int  $3
	jmp  2f
	.ascii "_main() returned."
2:	jmp  1b


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
