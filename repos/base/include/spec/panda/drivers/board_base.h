/*
 * \brief  Driver for the OMAP4 PandaBoard revision A2
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PANDA__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__SPEC__PANDA__DRIVERS__BOARD_BASE_H_

namespace Genode { struct Board_base; }


/**
 * Driver for the OMAP4 PandaBoard revision A2
 */
struct Genode::Board_base
{
	enum {

		/* device IO memory */
		MMIO_0_BASE = 0x48000000,
		MMIO_0_SIZE = 0x01000000,
		MMIO_1_BASE = 0x4a000000,
		MMIO_1_SIZE = 0x01000000,

		/* normal RAM */
		RAM_0_BASE = 0x80000000,
		RAM_0_SIZE = 0x40000000,

		/* clocks */
		SYS_CLK = 38400000,

		/* UART controllers */
		TL16C750_1_MMIO_BASE = MMIO_0_BASE + 0x6a000,
		TL16C750_2_MMIO_BASE = MMIO_0_BASE + 0x6c000,
		TL16C750_3_MMIO_BASE = MMIO_0_BASE + 0x20000,
		TL16C750_4_MMIO_BASE = MMIO_0_BASE + 0x6e000,
		TL16C750_MMIO_SIZE = 0x2000,
		TL16C750_CLOCK = 48*1000*1000,
		TL16C750_1_IRQ = 104,
		TL16C750_2_IRQ = 105,
		TL16C750_3_IRQ = 106,
		TL16C750_4_IRQ = 102,

		/* USB */
		HSUSB_EHCI_IRQ = 109,

		/* timer modules */
		GP_TIMER_3_IRQ = 71,

		/* CPU */
		CORTEX_A9_PRIVATE_MEM_BASE  = 0x48240000,
		CORTEX_A9_PRIVATE_MEM_SIZE  = 0x00002000,
		CORTEX_A9_PRIVATE_TIMER_CLK = 350000000,
		CORTEX_A9_PRIVATE_TIMER_DIV = 175,
		CORTEX_A9_WUGEN_MMIO_BASE   = 0x48281000,
		CORTEX_A9_SCU_MMIO_BASE     = 0x48240000,

		/* L2 cache */
		PL310_MMIO_BASE = 0x48242000,
		PL310_MMIO_SIZE = 0x00001000,

		/* display subsystem */
		DSS_MMIO_BASE   = 0x58000000,
		DSS_MMIO_SIZE   = 0x00001000,
		DISPC_MMIO_BASE = 0x58001000,
		DISPC_MMIO_SIZE = 0x00001000,
		HDMI_MMIO_BASE  = 0x58006000,
		HDMI_MMIO_SIZE  = 0x00001000,

		/* GPIO */
		GPIO1_MMIO_BASE = 0x4a310000,
		GPIO1_MMIO_SIZE = 0x1000,
		GPIO1_IRQ       = 29 + 32,
		GPIO2_MMIO_BASE = 0x48055000,
		GPIO2_MMIO_SIZE = 0x1000,
		GPIO2_IRQ       = 30 + 32,
		GPIO3_MMIO_BASE = 0x48057000,
		GPIO3_MMIO_SIZE = 0x1000,
		GPIO3_IRQ       = 31 + 32,
		GPIO4_MMIO_BASE = 0x48059000,
		GPIO4_MMIO_SIZE = 0x1000,
		GPIO4_IRQ       = 32 + 32,
		GPIO5_MMIO_BASE = 0x4805b000,
		GPIO5_MMIO_SIZE = 0x1000,
		GPIO5_IRQ       = 33 + 32,
		GPIO6_MMIO_BASE = 0x4805d000,
		GPIO6_MMIO_SIZE = 0x1000,
		GPIO6_IRQ       = 34 + 32,

		/* SD card */
		HSMMC_IRQ = 115,

		/* CPU cache */
		CACHE_LINE_SIZE_LOG2 = 2, /* FIXME get correct value from board spec */

		/* wether board provides security extension */
		SECURITY_EXTENSION = 0,
	};
};

#endif /* _INCLUDE__SPEC__PANDA__DRIVERS__BOARD_BASE_H_ */
