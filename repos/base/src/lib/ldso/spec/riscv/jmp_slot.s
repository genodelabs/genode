/**
 * \brief  Jump slot entry code for RISC-V
 * \author Sebastian Sumpf
 * \date   2015-09-07
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.text
.globl _jmp_slot
.type  _jmp_slot,%function
/*
 *  t0 = GOT[1] = Dependency
 *  t1 = PLT offset
 */
_jmp_slot:

	/* stack frame */
	addi sp, sp, -(8*9)

	/* save arguments and return address */
	sd a0, (0)(sp)
	sd a1, (1 * 8)(sp)
	sd a2, (2 * 8)(sp)
	sd a3, (3 * 8)(sp)
	sd a4, (4 * 8)(sp)
	sd a5, (5 * 8)(sp)
	sd a6, (6 * 8)(sp)
	sd a7, (7 * 8)(sp)
	sd ra, (8 * 8)(sp)

	/* GOT[1] = Dependency */
	mv a0, t0 /* arg 0 */

	/* calculate symbol index from PLT offset (index = offset / 8) */
	srli a1, t1, 3 /* arg 1 */

	jal  jmp_slot

	/* save address of function to call */
	mv t0, a0

	/* restore arguments and return address */
	ld a0, (0)(sp)
	ld a1, (1 * 8)(sp)
	ld a2, (2 * 8)(sp)
	ld a3, (3 * 8)(sp)
	ld a4, (4 * 8)(sp)
	ld a5, (5 * 8)(sp)
	ld a6, (6 * 8)(sp)
	ld a7, (7 * 8)(sp)
	ld ra, (8 * 8)(sp)

	/* stack frame */
	addi sp, sp,(8*9)

	/* call function */
	jr t0
