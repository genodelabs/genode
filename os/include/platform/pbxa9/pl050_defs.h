/*
 * \brief  PL050 PS/2 controller definitions for the RealView platform
 * \author Norman Feske
 * \date   2010-03-23
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__PBXA9__PL050_DEFS_H_
#define _INCLUDE__PLATFORM__PBXA9__PL050_DEFS_H_

enum {
	PL050_KEYBD_PHYS = 0x10006000, PL050_KEYBD_SIZE = 0x1000,
	PL050_MOUSE_PHYS = 0x10007000, PL050_MOUSE_SIZE = 0x1000,

	/**
	 * Interrupt lines
	 */
	PL050_KEYBD_IRQ = 20 + 32,
	PL050_MOUSE_IRQ = 21 + 32,
};

#endif /* _INCLUDE__PLATFORM__PBXA9__PL050_DEFS_H_ */
