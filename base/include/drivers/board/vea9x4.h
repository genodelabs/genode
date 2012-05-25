/*
 * \brief  Driver for the Versatile Express A9X4 board
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__BOARD__VEA9X4_H_
#define _INCLUDE__DRIVERS__BOARD__VEA9X4_H_

namespace Genode
{
	/**
	 * Driver for the Versatile Express A9X4 board
	 *
	 * Implies the uATX motherboard and the CoreTile Express A9X4 daughterboard
	 */
	struct Vea9x4
	{
		enum
		{
			/* static memory bus */
			SMB_CS7_BASE = 0x10000000,
			SMB_CS7_SIZE = 0x20000,
			SMB_CS0_TO_CS6_BASE = 0x40000000,
			SMB_CS0_TO_CS6_SIZE = 0x20000000,

			/* UART */
			PL011_0_MMIO_BASE = SMB_CS7_BASE + 0x9000,
			PL011_0_MMIO_SIZE = 0x1000,
			PL011_0_CLOCK = 24*1000*1000,
			PL011_0_IRQ = 5,

			/* timer/counter */
			SP804_0_1_MMIO_BASE = SMB_CS7_BASE + 0x11000,
			SP804_0_1_MMIO_SIZE = 0x1000,
			SP804_0_1_CLOCK = 1000*1000,
			SP804_0_1_IRQ = 34,

			/* clocks */
			TCREF_CLOCK = 66670*1000,

			/* CPU */
			CORTEX_A9_PRIVATE_MEM_BASE = 0x1e000000,
			CORTEX_A9_PRIVATE_MEM_SIZE = 0x2000,
			CORTEX_A9_CLOCK = TCREF_CLOCK,
			CORTEX_A9_SECURITY_EXTENSION = 1,

			/* RAM */
			LOCAL_DDR2_BASE = 0x60000000,
			LOCAL_DDR2_SIZE = 0x40000000,
		};
	};
}

#endif /* _INCLUDE__DRIVERS__BOARD__VEA9X4_H_ */

