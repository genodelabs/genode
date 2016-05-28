/**
 * \brief  Driver for the Realview PBXA9 board
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SPEC__PBXA9__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__SPEC__PBXA9__DRIVERS__BOARD_BASE_H_

namespace Genode { struct Board_base; }


/**
 * Driver for the Realview PBXA9 board
 */
struct Genode::Board_base
{
	enum {

		/* normal RAM */
		RAM_0_BASE = 0x70000000,
		RAM_0_SIZE = 0x20000000,
		RAM_1_BASE = 0x20000000,
		RAM_1_SIZE = 0x10000000,

		/* device IO memory */
		MMIO_0_BASE = 0x10000000,
		MMIO_0_SIZE = 0x10000000,
		MMIO_1_BASE = 0x4e000000,
		MMIO_1_SIZE = 0x01000000,

		NORTHBRIDGE_AHB_BASE = 0x10020000,
		NORTHBRIDGE_AHB_SIZE = 768*1024,

		/* southbridge */
		SOUTHBRIDGE_APB_BASE = 0x10000000,
		SOUTHBRIDGE_APB_SIZE = 128*1024,

		/* clocks */
		OSC_6_CLOCK =  24*1000*1000,

		/* system controller */
		SYSTEM_CONTROL_MMIO_BASE = 0x10000000,

		/* CPU */
		CORTEX_A9_PRIVATE_TIMER_CLK = 100000000,
		CORTEX_A9_PRIVATE_TIMER_DIV = 100,
		CORTEX_A9_PRIVATE_MEM_BASE  = 0x1f000000,
		CORTEX_A9_PRIVATE_MEM_SIZE  = 0x2000,

		/* L2 cache controller */
		PL310_MMIO_BASE = 0x1f002000,
		PL310_MMIO_SIZE = 0x00001000,

		/* UART */
		PL011_0_MMIO_BASE = 0x10009000,
		PL011_0_MMIO_SIZE = 0x00001000,
		PL011_0_CLOCK     = OSC_6_CLOCK,
		PL011_0_IRQ       = 44,
		PL011_1_IRQ       = 45,
		PL011_2_IRQ       = 46,
		PL011_3_IRQ       = 47,

		/* timer */
		SP804_0_1_MMIO_BASE = 0x10011000,
		SP804_0_1_MMIO_SIZE = 0x00001000,
		SP804_0_1_IRQ       = 36,
		SP804_0_1_CLOCK     = 1000*1000,

		/* keyboard & mouse */
		KMI_0_IRQ = 52,
		KMI_1_IRQ = 53,

		/* LAN */
		ETHERNET_IRQ = 60,

		/* SD card */
		PL180_IRQ_0 = 49,
		PL180_IRQ_1 = 50,

		/* CPU cache */
		CACHE_LINE_SIZE_LOG2 = 2, /* FIXME get correct value from board spec */

		/* wether board provides security extension */
		SECURITY_EXTENSION = 0,
	};
};

#endif /* _INCLUDE__SPEC__PBXA9__DRIVERS__BOARD_BASE_H_ */
