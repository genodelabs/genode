/**
 * \brief  Jump slot entry code for RISC-V
 * \author Sebastian Sumpf
 * \date   2015-09-07
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
	jal jmp_slot
