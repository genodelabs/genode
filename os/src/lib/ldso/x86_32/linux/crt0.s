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

	.globl _start_ldso
_start_ldso:

	/* set global offset table the taditional way */
	call 3f
3:
	popl %ebx;
	addl $_GLOBAL_OFFSET_TABLE_+ (. - 3b),  %ebx

	movl %esp, __initial_sp@GOTOFF(%ebx)

	/*
	 * environ = &argv[argc + 1]
	 * in Genode argc is always 1
	 */
	popl %eax /* argc */
	popl %eax /* argv[0] */
	popl %eax /* NULL */
	movl %esp, lx_environ@GOTOFF(%ebx)

	/* XXX Switch to our own stack.  */
	leal _stack_high@GOTOFF(%ebx), %esp

	/* relocate ldso */
	call init_rtld

	/* Clear the base pointer so that stack backtraces will work.  */
	xorl %ebp,%ebp

	/* Jump into init C code */
	call _main

	/* We should never get here since _main does not return */
1:	int  $3
	jmp  2f
	.ascii "_main() returned."
2:	jmp  1b

