/*
 * \brief  Driver for the OMAP4 PandaBoard revision A2
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__DRIVERS__BOARD_BASE_H_

namespace Genode
{
	/**
	 * Driver for the OMAP4 PandaBoard revision A2
	 */
	struct Board_base
	{
		enum
		{
			/* device IO memory */
			MMIO_0_BASE = 0x48000000,
			MMIO_0_SIZE = 0x01000000,
			MMIO_1_BASE = 0x4a000000,
			MMIO_1_SIZE = 0x01000000,

			/* normal RAM */
			RAM_0_BASE = 0x80000000,
			RAM_0_SIZE = 0x40000000,

			/* clocks */
			MPU_DPLL_CLOCK = 200*1000*1000,

			/* UARTs */
			TL16C750_1_MMIO_BASE = MMIO_0_BASE + 0x6a000,
			TL16C750_2_MMIO_BASE = MMIO_0_BASE + 0x6c000,
			TL16C750_3_MMIO_BASE = MMIO_0_BASE + 0x20000,
			TL16C750_4_MMIO_BASE = MMIO_0_BASE + 0x6e000,

			TL16C750_MMIO_SIZE = 0x2000,
			TL16C750_CLOCK = 48*1000*1000,

			TL16C750_1_IRQ = 72,
			TL16C750_2_IRQ = 73,
			TL16C750_3_IRQ = 74,
			TL16C750_4_IRQ = 70,

			/* CPU */
			CORTEX_A9_PRIVATE_MEM_BASE = 0x48240000,
			CORTEX_A9_PRIVATE_MEM_SIZE = 0x00002000,
			CORTEX_A9_CLOCK = MPU_DPLL_CLOCK,

			/* display subsystem */
			DSS_MMIO_BASE   = 0x58000000,
			DSS_MMIO_SIZE   = 0x00001000,
			DISPC_MMIO_BASE = 0x58001000,
			DISPC_MMIO_SIZE = 0x00001000,
			HDMI_MMIO_BASE  = 0x58006000,
			HDMI_MMIO_SIZE  = 0x00001000,

			/* misc */
			SECURITY_EXTENSION = 0,
			SYS_CLK            = 38400000,
		};
	};
}

#endif /* _INCLUDE__DRIVERS__BOARD_BASE_H_ */

