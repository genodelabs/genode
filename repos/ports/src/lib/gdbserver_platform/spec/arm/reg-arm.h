/*
 * \brief  Genode backend for GDB server (ARM-specific code)
 * \author Christian Prochaska
 * \date   2011-08-01
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef REG_ARM_H
#define REG_ARM_H

/* indices from 'regs_arm' array in reg-arm.c */
enum reg_index {
	R0   = 0,
	R1   = 1,
	R2   = 2,
	R3   = 3,
	R4   = 4,
	R5   = 5,
	R6   = 6,
	R7   = 7,
	R8   = 8,
	R9   = 9,
	R10  = 10,
	R11  = 11,
	R12  = 12,
	SP   = 13,
	LR   = 14,
	PC   = 15,
	F0   = 16,
	F1   = 17,
	F2   = 18,
	F3   = 19,
	F4   = 20,
	F5   = 21,
	F6   = 22,
	F7   = 23,
	FPS  = 24,
	CPSR = 25,
};

#endif /* REG_ARM_H */
