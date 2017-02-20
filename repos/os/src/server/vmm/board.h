/*
 * \brief  Driver for the Versatile Express A9X4 board
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PLATFORM__VEA9X4__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__PLATFORM__VEA9X4__DRIVERS__BOARD_BASE_H_

namespace Vea9x4 { struct Board; }


/**
 * Driver for the Versatile Express A9X4 board
 *
 * Implies the uATX motherboard and the CoreTile Express A9X4 daughterboard
 */
struct Vea9x4::Board
{
	enum
	{
		/* MMIO */
		MMIO_0_BASE = 0x10000000,
		MMIO_0_SIZE = 0x10000000,
		MMIO_1_BASE = 0x4C000000,
		MMIO_1_SIZE = 0x04000000,

		/* RAM */
		RAM_0_BASE = 0x60000000,
		RAM_0_SIZE = 0x20000000,
		RAM_1_BASE = 0x84000000,
		RAM_1_SIZE = 0x1c000000,
		RAM_2_BASE = 0x48000000,
		RAM_2_SIZE = 0x02000000,

		/* UART */
		PL011_0_MMIO_BASE = MMIO_0_BASE + 0x9000,
		PL011_0_MMIO_SIZE = 0x1000,
		PL011_0_CLOCK = 24*1000*1000,
		PL011_0_IRQ = 37,
		PL011_1_IRQ = 38,
		PL011_2_IRQ = 39,
		PL011_3_IRQ = 40,

		/* timer/counter */
		SP804_0_1_MMIO_BASE = MMIO_0_BASE + 0x11000,
		SP804_0_1_MMIO_SIZE = 0x1000,
		SP804_0_1_CLOCK = 1000*1000,
		SP804_0_1_IRQ = 34,

		/* PS2 */
		KMI_0_IRQ = 44,
		KMI_1_IRQ = 45,

		/* LAN */
		LAN9118_IRQ = 47,

		/* card reader */
		PL180_0_IRQ = 9,
		PL180_1_IRQ = 10,

		/* CPU */
		CORTEX_A9_PRIVATE_MEM_BASE  = 0x1e000000,
		CORTEX_A9_PRIVATE_MEM_SIZE  = 0x2000,
		CORTEX_A9_PRIVATE_TIMER_CLK = 200010000,

		/* wether board provides security extension */
		SECURITY_EXTENSION = 1,

		/* CPU cache */
		CACHE_LINE_SIZE_LOG2 = 2, /* FIXME get correct value from board spec */
	};
};

#endif /* _INCLUDE__PLATFORM__VEA9X4__DRIVERS__BOARD_BASE_H_ */

