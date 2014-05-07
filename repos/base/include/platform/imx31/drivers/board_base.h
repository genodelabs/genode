/*
 * \brief  Board definitions for the i.MX31
 * \author Martin Stein
 * \author Norman Feske
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__BOARD_BASE_H_
#define _INCLUDE__PLATFORM__BOARD_BASE_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{
	/**
	 * i.MX31 motherboard
	 */
	struct Board_base
	{
		enum {
			RAM_0_BASE = 0x80000000,
			RAM_0_SIZE = 0x20000000,

			MMIO_0_BASE = 0x30000000,
			MMIO_0_SIZE = 0x50000000,

			UART_1_IRQ       = 45,
			UART_1_MMIO_BASE = 0x43f90000,
			UART_1_MMIO_SIZE = 0x00004000,

			EPIT_1_IRQ       = 28,
			EPIT_1_MMIO_BASE = 0x53f94000,
			EPIT_1_MMIO_SIZE = 0x00004000,

			EPIT_2_IRQ       = 27,
			EPIT_2_MMIO_BASE = 0x53f98000,
			EPIT_2_MMIO_SIZE = 0x00004000,

			AVIC_MMIO_BASE = 0x68000000,
			AVIC_MMIO_SIZE = 0x04000000,

			AIPS_1_MMIO_BASE = 0x43F00000,
			AIPS_1_MMIO_SIZE = 0x00004000,

			AIPS_2_MMIO_BASE = 0x53F00000,
			AIPS_2_MMIO_SIZE = 0x00004000,

			/* CPU cache */
			CACHE_LINE_SIZE_LOG2 = 2, /* FIXME get correct value from board spec */
		};
	};
}

#endif /* _INCLUDE__PLATFORM__BOARD_BASE_H_ */

