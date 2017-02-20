/*
 * \brief  PL180 multi-media-card interface definitions for the RealView platform
 * \author Christian Helmuth
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PBXA9__PL180_DEFS_H_
#define _INCLUDE__SPEC__PBXA9__PL180_DEFS_H_

#include <drivers/board_base.h>

enum {
	PL180_PHYS = 0x10005000, PL180_SIZE = 0x1000,

	/**
	 * Interrupt lines
	 */
	PL180_IRQ0 = Genode::Board_base::PL180_IRQ_0,
	PL180_IRQ1 = Genode::Board_base::PL180_IRQ_1,
};

#endif /* _INCLUDE__SPEC__PBXA9__PL180_DEFS_H_ */
