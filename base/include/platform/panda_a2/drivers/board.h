/*
 * \brief  Driver for the OMAP4 PandaBoard revision A2
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
	 * Driver for the OMAP4 PandaBoard revision A2
	 */
	struct Board
	{
		enum
		{
			/* interconnect domains */
			L4_PER_BASE = 0x48000000,
			L4_PER_SIZE = 0x01000000,
			L4_CFG_BASE = 0x4a000000,
			L4_CFG_SIZE = 0x01000000,

			/* clocks */
			MPU_DPLL_CLOCK = 200*1000*1000,

			/* UART */
			TL16C750_3_MMIO_BASE = L4_PER_BASE + 0x20000,
			TL16C750_3_MMIO_SIZE = 0x2000,
			TL16C750_3_CLOCK = 48*1000*1000,
			TL16C750_3_IRQ = 74,

			/* CPU */
			CORTEX_A9_PRIVATE_MEM_BASE = L4_PER_BASE + 0x240000,
			CORTEX_A9_PRIVATE_MEM_SIZE = 0x2000,
			CORTEX_A9_CLOCK = MPU_DPLL_CLOCK,

			/* RAM */
			EMIF1_EMIF2_CS0_SDRAM_BASE = 0x80000000,
			EMIF1_EMIF2_CS0_SDRAM_SIZE = 0x40000000,

			/* display subsystem */
			DSS_MMIO_BASE   = 0x58000000,
			DSS_MMIO_SIZE   = 0x00001000,
			DISPC_MMIO_BASE = 0x58001000,
			DISPC_MMIO_SIZE = 0x1000,
			HDMI_MMIO_BASE  = 0x58006000,
			HDMI_MMIO_SIZE  = 0x1000,

			/* misc */
			SECURITY_EXTENSION = 0,
			SYS_CLK            = 38400000,
		};
	};
}

#endif /* _INCLUDE__DRIVERS__BOARD_H_ */

