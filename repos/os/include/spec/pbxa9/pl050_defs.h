/*
 * \brief  PL050 PS/2 controller definitions for the RealView platform
 * \author Norman Feske
 * \date   2010-03-23
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PBXA9__PL050_DEFS_H_
#define _INCLUDE__SPEC__PBXA9__PL050_DEFS_H_

#include <drivers/board_base.h>

enum {
	PL050_KEYBD_PHYS = 0x10006000, PL050_KEYBD_SIZE = 0x1000,
	PL050_MOUSE_PHYS = 0x10007000, PL050_MOUSE_SIZE = 0x1000,

	/**
	 * Interrupt lines
	 */
	PL050_KEYBD_IRQ = Genode::Board_base::KMI_0_IRQ,
	PL050_MOUSE_IRQ = Genode::Board_base::KMI_1_IRQ,
};

#endif /* _INCLUDE__SPEC__PBXA9__PL050_DEFS_H_ */
