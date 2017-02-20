/**
 * \brief  Jump slot entry code for x86_32
 * \author Sebastian Sumpf
 * \date   2014-10-26
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.text
.align 4
.globl _jmp_slot
.type _jmp_slot,@function
_jmp_slot:
	pushf
	pushl %eax
	pushl %edx
	pushl %ecx
	pushl 0x14(%esp)      /* relocation index */
	pushl 0x14(%esp)      /* obj pointer */

	call jmp_slot@PLT

	addl $8, %esp         /* remove arguments from call  */
	movl %eax, 0x14(%esp) /* store synmbol value in obj pointer position  */
	popl %ecx
	popl %edx
	popl %eax
	popf
	leal 0x4(%esp), %esp  /* remove relocation index */
	ret                   /* return to symbol value */
