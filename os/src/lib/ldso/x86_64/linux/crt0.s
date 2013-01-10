/*
 * \brief   Startup code for ldso 64Bit-Linux version
 * \author  Christian Helmuth
 * \author  Sebastian Sumpf
 * \date    2011-05-10
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
	.text

	.globl _start_ldso
_start_ldso:

	/* initialize GLOBAL OFFSET TABLE */
	leaq _GLOBAL_OFFSET_TABLE_(%rip),%r15
	
	movq __initial_sp@GOTPCREL(%rip), %rax
	movq %rsp, (%rax)
	/*
	 * environ = &argv[argc + 1]
	 * in Genode argc is always 1
	 */
	popq %rax /* argc */
	popq %rax /* argv[0] */
	popq %rax /* NULL */
	movq lx_environ@GOTPCREL(%rip), %rax
	movq %rsp, (%rax)

	/* XXX Switch to our own stack.  */
	leaq _stack_high@GOTPCREL(%rip),%rax
	movq (%rax), %rsp

	call init_rtld

	/* Clear the base pointer so that stack backtraces will work.  */
	xorq %rbp,%rbp

	/* Jump into init C code */
	call _main

	/* We should never get here since _main does not return */
1:	int  $3
	jmp  2f
	.ascii "_main() returned."
2:	jmp  1b
