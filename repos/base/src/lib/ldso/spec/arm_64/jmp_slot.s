/**
 * \brief  Jump slot entry code for ARM 64-bit platforms
 * \author Sebastian Sumpf
 * \date   2019-04-04
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.text
.globl _jmp_slot
.type  _jmp_slot,%function
/*
 * sp + 0 = &GOT[n + 3]
 * sp + 8 = return address
 * x30    = return address
 * x16    = &GOT[2] // 'jmp_slot'
 * x17    = '_jmp_slot' (this function)
 */
_jmp_slot:

	mov x17, sp

	/* save fp, lr */
	stp x29, x30, [sp, #-16]!

	/* save arguments */
	stp x0, x1,  [sp, #-16]!
	stp x2, x3,  [sp, #-16]!
	stp x4, x5,  [sp, #-16]!
	stp x6, x7,  [sp, #-16]!
	stp x8, xzr, [sp, #-16]!

	/* GOT[1] = Dependency */
	ldr x0, [x16, #-8] /* arg 0 */

	/* calculate symbol index from GOT offset (arg 1) */
	ldr x2, [x17, #0] /* &GOT[n+3] */
	sub x1, x2, x16   /* GOT offset */
	sub x1, x1, #8    /* GOT[3] is first entry */

	/* a GOT entry is 8 bytes, therefore index = offset / 8 */
	lsr x1, x1, #3 /* x1 / 8 */

	bl jmp_slot

	/* address of function to call */
	mov x17, x0

	/* restore arguments */
	ldp x8, xzr, [sp], #16
	ldp x6, x7,  [sp], #16
	ldp x4, x5,  [sp], #16
	ldp x2, x3,  [sp], #16
	ldp x0, x1,  [sp], #16

	/* frame pointer */
	ldp x29, xzr, [sp], #16

	/* restore lr and close frame from plt code */
	ldp xzr, x30, [sp], #16

	/* call function */
	br x17
