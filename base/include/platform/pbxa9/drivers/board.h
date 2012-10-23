/**
 * \brief  Driver for the Realview PBXA9 board
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__BOARD_H_
#define _INCLUDE__DRIVERS__BOARD_H_

namespace Genode
{
	/**
	 * Driver for the Realview PBXA9 board
	 */
	struct Board
	{
		enum
		{
			/* northbridge */
			NORTHBRIDGE_DDR_0_BASE = 0x00000000,  /* DMC mirror */
			NORTHBRIDGE_DDR_0_SIZE = 256*1024*1024,

			NORTHBRIDGE_AHB_BASE = 0x10020000,
			NORTHBRIDGE_AHB_SIZE = 768*1024,

			/* southbridge */
			SOUTHBRIDGE_APB_BASE = 0x10000000,
			SOUTHBRIDGE_APB_SIZE = 128*1024,

			/* clocks */
			OSC_6_CLOCK =  24*1000*1000,
			OSC_7_CLOCK =  14*1000*1000,

			/* CPU */
			CORTEX_A9_CLOCK = OSC_7_CLOCK * 5,
			CORTEX_A9_PRIVATE_MEM_BASE = 0x1f000000,
			CORTEX_A9_PRIVATE_MEM_SIZE = 0x01000000,

			/* UART */
			PL011_0_MMIO_BASE = SOUTHBRIDGE_APB_BASE + 0x9000,
			PL011_0_MMIO_SIZE = 4*1024,
			PL011_0_CLOCK     = OSC_6_CLOCK,
			PL011_0_IRQ       = 44,

			/* timer */
			SP804_0_1_MMIO_BASE = SOUTHBRIDGE_APB_BASE + 0x11000,
			SP804_0_1_MMIO_SIZE = 4*1024,
			SP804_0_1_IRQ       = 36,
			SP804_0_1_CLOCK     = 1000*1000,

			SECURITY_EXTENSION = 0,
		};
	};
}

#endif /* _INCLUDE__DRIVERS__BOARD_H_ */

