/**
 * \brief   Startup code for ld.lib.so (x86-32)
 * \author  Christian Helmuth
 * \author  Sebastian Sumpf
 * \date    2011-05-03
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*--- .text (program code) -------------------------*/
	.text
	.global _start_ldso

_start_ldso:

	/* initialize GOT pointer in EBX */
3:
	/* The follwing statement causes a text relocation which will be ignored by
	 * ldso itself, this is necessary since we don't have a valid stack pointer at
	 * this moment so a 'call' in order to retrieve our IP and thus calculate the
	 * GOT-position in the traditional manner is not possible on x86
	 */
	movl $., %ebx
	addl $_GLOBAL_OFFSET_TABLE_ + (. - 3b) , %ebx

	movl %esp, __initial_sp@GOTOFF(%ebx)

	/* XXX Switch to our own stack.  */
	leal _stack_high@GOTOFF(%ebx), %esp

	/* relocate ldso */
	call init_rtld

	/* Clear the base pointer so that stack backtraces will work.  */
	xor %ebp,%ebp

	/* Jump into init C code */
	call _main

	/* We should never get here since _main does not return */
1:	int  $3
	jmp  2f
	.ascii "_main() returned."
2:	jmp  1b

