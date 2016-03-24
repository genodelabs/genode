/*
 * \brief  Base driver for Xilinx Zynq platforms
 * \author Mark Albers
 * \author Timo Wischer
 * \author Johannes Schlatow
 * \date   2014-12-15
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SPEC__ZYNQ__DRIVERS__BOARD_BASE_SUPPORT_H_
#define _INCLUDE__SPEC__ZYNQ__DRIVERS__BOARD_BASE_SUPPORT_H_

namespace Zynq { struct Board_base; }

/**
 * Base driver for the Zynq platform
 */
struct Zynq::Board_base
{
	enum
	{
		/* device IO memory */
		MMIO_0_BASE    = 0xe0000000, /* IOP devices */
		MMIO_0_SIZE    = 0x10000000,
		MMIO_1_BASE    = 0xF8000000, /* Programmable register via APB */
		MMIO_1_SIZE    = 0x02000000,
		QSPI_MMIO_BASE = 0xFC000000, /* Quad-SPI */
		QSPI_MMIO_SIZE = 0x01000000,
		OCM_MMIO_BASE  = 0xFFFC0000, /* OCM upper address range */
		OCM_MMIO_SIZE  = 0x00040000,

		/* normal RAM */
		RAM_0_BASE = 0x00000000,
		RAM_0_SIZE = 0x40000000, /* 1GiB */

		/* AXI */
		AXI_0_MMIO_BASE = 0x40000000, /* PL AXI Slave port #0 */
		AXI_0_MMIO_SIZE = 0x40000000,
		AXI_1_MMIO_BASE = 0x80000000, /* PL AXI Slave port #1 */
		AXI_1_MMIO_SIZE = 0x40000000,

		/* UART controllers */
		UART_0_MMIO_BASE = MMIO_0_BASE,
		UART_1_MMIO_BASE = MMIO_0_BASE + 0x1000,
		UART_SIZE        = 0x1000,
		UART_CLOCK       = 50*1000*1000,
		UART_0_IRQ       = 59,
		UART_1_IRQ       = 82,

		/* CPU */
		CORTEX_A9_PRIVATE_MEM_BASE  = 0xf8f00000,
		CORTEX_A9_PRIVATE_MEM_SIZE  = 0x00002000,

		/* CPU cache */
		PL310_MMIO_BASE      = MMIO_1_BASE + 0xF02000,
		PL310_MMIO_SIZE      = 0x1000,
		CACHE_LINE_SIZE_LOG2 = 2, /* FIXME get correct value from board spec */

		/* TTC (triple timer counter) */
		TTC0_MMIO_BASE = MMIO_1_BASE + 0x1000,
		TTC0_MMIO_SIZE = 0xfff,
		TTC1_MMIO_BASE = MMIO_1_BASE + 0x2000,
		TTC1_MMIO_SIZE = 0xfff,
		TTC0_IRQ_0     = 42,
		TTC0_IRQ_1     = 43,
		TTC0_IRQ_2     = 44,
		TTC1_IRQ_0     = 69,
		TTC1_IRQ_1     = 70,
		TTC1_IRQ_2     = 71,

		/* Ethernet MAC PS */
		EMAC_0_MMIO_BASE   = 0xE000B000,
		EMAC_0_MMIO_SIZE   = 0x1000,
		EMAC_0_IRQ         = 54,

		/* SD host controller interface */
		SDHCI_0_BASE         = MMIO_0_BASE + 0x100000,
		SDHCI_0_SIZE         = 0x100,
		SDHCI_0_IRQ          = 56,

		SDHCI_1_BASE         = MMIO_0_BASE + 0x101000,
		SDHCI_1_SIZE         = 0x100,
		SDHCI_1_IRQ          = 79,

		/* wether board provides security extension */
		SECURITY_EXTENSION = 0,
	};
};

#endif /* _INCLUDE__SPEC__ZYNQ__DRIVERS__BOARD_BASE_SUPPORT_H_ */
