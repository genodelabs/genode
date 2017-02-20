/**
 * \brief  Jump slot entry code for ARM platforms
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
.globl _jmp_slot
.type  _jmp_slot,%function
/*
 *  stack[0] = RA
 *  ip = &GOT[n+3]
 *  lr = &GOT[2]
 */
_jmp_slot:

	stmdb sp!,{r0-r4,sl,fp}

	/* retrieve relocation index */
	sub r1, ip, lr             /* r1 = 4 * (n + 1) */
	sub r1, r1, #4             /* r1 = 4 * n       */
	lsr r1, r1, #2             /* r1 = n           */

	ldr r0, [lr, #-4]          /* obj pointer from PLOTGOT[1] */
	mov r4, ip                 /* safe got location */

	bl jmp_slot                /* call linker(obj, index) */

	str   r0, [r4]             /* save symbol value in GOT */
	mov   ip, r0
	ldmia sp!,{r0-r4,sl,fp,lr} /* restore the stack */
	mov   pc, ip               /* jump to symbol address */
