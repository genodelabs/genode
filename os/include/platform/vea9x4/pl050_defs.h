/*
 * \brief  PL050 PS/2 controller definitions for the RealView platform
 * \author Norman Feske
 * \author Stefan Kalkowski
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

#include <platform/vea9x4/bus.h>

enum {
	PL050_KEYBD_PHYS = SMB_CS7 + 0x6000, PL050_KEYBD_SIZE = 0x1000,
	PL050_MOUSE_PHYS = SMB_CS7 + 0x7000, PL050_MOUSE_SIZE = 0x1000,

	/**
	 * Interrupt lines
	 */
	PL050_KEYBD_IRQ = 32 + 12,
	PL050_MOUSE_IRQ = 32 + 13,
};

#endif /* _INCLUDE__PLATFORM__PBXA9__PL050_DEFS_H_ */
