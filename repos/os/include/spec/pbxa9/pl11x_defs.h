/*
 * \brief  PL111 display controller definitions for the RealView platform
 * \author Norman Feske
 * \date   2010-03-23
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PBXA9__PL11X_DEFS_H_
#define _INCLUDE__SPEC__PBXA9__PL11X_DEFS_H_

enum {
	PL11X_LCD_PHYS = 0x10020000,
	PL11X_LCD_SIZE = 0x1000,

	/**
	 * Offsets of LCD control register offsets (in 32bit words)
	 */
	PL11X_REG_TIMING0 = 0,
	PL11X_REG_TIMING1 = 1,
	PL11X_REG_TIMING2 = 2,
	PL11X_REG_TIMING3 = 3,
	PL11X_REG_UPBASE  = 4,
	PL11X_REG_LPBASE  = 5,
	PL11X_REG_CTRL    = 6,
	PL11X_REG_IMSC    = 7,
};

#endif /* _INCLUDE__SPEC__PBXA9__PL11X_DEFS_H_ */
