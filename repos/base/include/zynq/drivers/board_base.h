/*
 * \brief  Base driver for the Zynq (QEMU)
 * \author Mark Albers
 * \author Timo Wischer
 * \author Johannes Schlatow
 * \date   2014-12-15
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__DRIVERS__BOARD_BASE_H_

namespace Genode
{
	/**
	 * Base driver for the Zynq platform
	 */
	struct Board_base
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
			I2C0_MMIO_BASE = 0xE0004000,
			I2C1_MMIO_BASE = 0xE0005000,
			I2C_MMIO_SIZE = 0x1000,

			/* normal RAM */
			RAM_0_BASE = 0x00000000,
			RAM_0_SIZE = 0x40000000, /* 1GiB */

			/* AXI */
			AXI_0_MMIO_BASE = 0x40000000, /* PL AXI Slave port #0 */
			AXI_0_MMIO_SIZE = 0x40000000,
			AXI_1_MMIO_BASE = 0x80000000, /* PL AXI Slave port #1 */
			AXI_1_MMIO_SIZE = 0x40000000,

			/* clocks (assuming 6:2:1 mode) */
			PS_CLOCK = 33333333,
			ARM_PLL_CLOCK = 1333333*1000,
			DDR_PLL_CLOCK = 1066667*1000,
			IO_PLL_CLOCK  = 1000*1000*1000,
			CPU_1x_CLOCK = 111111115,
			CPU_6x4x_CLOCK = 6*CPU_1x_CLOCK,
			CPU_3x2x_CLOCK = 3*CPU_1x_CLOCK,
			CPU_2x_CLOCK   = 2*CPU_1x_CLOCK,

			/* UART controllers */
			UART_0_MMIO_BASE = MMIO_0_BASE + 0x0000,
			UART_1_MMIO_BASE = MMIO_0_BASE + 0x1000,
			UART_SIZE = 0x1000,
			UART_CLOCK = 50*1000*1000,
			UART_0_IRQ = 59,
			UART_1_IRQ = 82,

			/* DDR */
			DDR_CLOCK  = 400*1000*1000,
			QSPI_CLOCK = 200*1000*1000,
			ETH_CLOCK  = 125*1000*1000,
			SD_CLOCK   =  50*1000*1000,

			FCLK_CLK0  = 100*1000*1000, /* AXI */
			FCLK_CLK1  = 200*1000*1000, /* AXI VDMA */
                    	FCLK_CLK2  = 200*1000*1000, /* HDMI */
                       	FCLK_CLK3  =  40*1000*1000, /* Epiphany */

			/* CPU */
			CORTEX_A9_PRIVATE_MEM_BASE = 0xf8f00000,
			CORTEX_A9_PRIVATE_MEM_SIZE = 0x00002000,
			CORTEX_A9_CLOCK = CPU_6x4x_CLOCK,
			CORTEX_A9_PRIVATE_TIMER_CLK = CORTEX_A9_CLOCK, /* FIXME not exactly sure about this */

			/* CPU cache */
			PL310_MMIO_BASE = MMIO_1_BASE + 0xF02000,
			PL310_MMIO_SIZE = 0x1000,
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
			EMAC_0_MMIO_BASE	= 0xE000B000,
			EMAC_0_MMIO_SIZE	= 0x1000,
			EMAC_0_IRQ			= 54,
			EMAC_0_IRQ_WAKE_UP	= 55,

			/* GPIO */
			GPIO_MMIO_SIZE = 0x1000,

			/* VDMA */
			VDMA_MMIO_SIZE = 0x10000,

			/* wether board provides security extension */
			SECURITY_EXTENSION = 0,

		};
	};
}

#endif /* _INCLUDE__DRIVERS__BOARD_BASE_H_ */

