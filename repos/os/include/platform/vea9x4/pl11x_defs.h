/*
 * \brief  PL111 display controller definitions for the RealView platform
 * \author Stefan Kalkowski
 * \date   2011-11-08
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__VEA9X4__PL11X_DEFS_H_
#define _INCLUDE__PLATFORM__VEA9X4__PL11X_DEFS_H_

#include <platform/vea9x4/bus.h>

enum {
	PL11X_LCD_PHYS  = SMB_CS7 + 0x1f000,
	PL11X_LCD_SIZE  = 0x1000,

	PL11X_VIDEO_RAM = SMB_CS3,

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

#endif /* _INCLUDE__PLATFORM__VEA9X4__PL11X_DEFS_H_ */
