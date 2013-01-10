/*
 * \brief  PL180 multi-media-card interface definitions for the RealView platform
 * \author Christian Helmuth
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__PBXA9__PL180_DEFS_H_
#define _INCLUDE__PLATFORM__PBXA9__PL180_DEFS_H_

enum {
	PL180_PHYS = 0x10005000, PL180_SIZE = 0x1000,

	/**
	 * Interrupt lines
	 */
	PL180_IRQ0 = 17 + 32,
	PL180_IRQ1 = 18 + 32,
};

#endif /* _INCLUDE__PLATFORM__PBXA9__PL180_DEFS_H_ */
