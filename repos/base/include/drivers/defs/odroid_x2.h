/*
 * \brief  MMIO and IRQ definitions of the Odroid-x2 board
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto López León <humberto@uclv.cu>
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-07-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__ODROID_X2_H_
#define _INCLUDE__DRIVERS__DEFS__ODROID_X2_H_

namespace Odroid_x2 {
	enum
	{
		/* clock management unit */
		CMU_MMIO_BASE = 0x10030000,
		CMU_MMIO_SIZE = 0x18000,

		/* power management unit */
		PMU_MMIO_BASE = 0x10020000,
		PMU_MMIO_SIZE = 0x5000, /* TODO Check the region size */

		/* UART */
		UART_1_MMIO_BASE = 0x13810000,
		UART_1_IRQ       = 85,
		UART_1_CLOCK     = 100000000, /* TODO Check SCLK_UART1 */

		UART_2_MMIO_BASE = 0x13820000,
		UART_2_IRQ       = 86,
		UART_2_CLOCK     = 100000000, /* TODO Check SCLK_UART2 */

		MCT_IRQ_L0    = 28,
		MCT_IRQ_L1    = 28,
		MCT_IRQ_L2    = 28,
		MCT_IRQ_L3    = 28,

		TIMER_IRQ = 28,

		/* USB IRQ */
		USB_HOST20_IRQ = 102,


		/******************************
		 ** HDMI memory map and irqs **
		 ******************************/

		/* Mixer base */
		MIXER_BASE = 0x12C10000,

		/* HDMI base */
		HDMI_BASE  = 0x12D00000,

		/* IC2 BASE */
		I2C_BASE = 0x138E0000,

		/* HDMI IRQ */
		I2C_HDMI_IRQ = 125,

		/* GPIO */
		GPIO1_MMIO_BASE = 0x11400000,
		GPIO1_MMIO_SIZE = 0x0F88,
		GPIO1_IRQ       = 79, /* TODO Check the irq number */

		GPIO2_MMIO_BASE = 0x11000000,
		GPIO2_MMIO_SIZE = 0x0F88,
		GPIO2_IRQ       = 79, /* TODO Check the irq number */

		GPIO3_MMIO_BASE = 0x03860000,
		GPIO3_MMIO_SIZE = 0x0F88,
		GPIO3_IRQ       = 79, /* TODO Check the irq number */

		GPIO4_MMIO_BASE = 0x106E0000,
		GPIO4_MMIO_SIZE = 0x0F88,
		GPIO4_IRQ       = 79, /* TODO Check the irq number */
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__ODROID_X2_H_ */
