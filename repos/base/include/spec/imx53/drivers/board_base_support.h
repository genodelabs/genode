/*
 * \brief  Board definitions for the i.MX53
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__IMX53__DRIVERS__BOARD_BASE_SUPPORT_H_
#define _INCLUDE__SPEC__IMX53__DRIVERS__BOARD_BASE_SUPPORT_H_

namespace Imx53 { struct Board_base; }


/**
 * i.MX53 motherboard
 */
struct Imx53::Board_base
{
	enum {
		MMIO_BASE          = 0x0,
		MMIO_SIZE          = 0x70000000,

		SDHC_IRQ       = 1,
		SDHC_MMIO_BASE = 0x50004000,
		SDHC_MMIO_SIZE = 0x00004000,

		UART_1_IRQ         = 31,
		UART_1_MMIO_BASE   = 0x53fbc000,
		UART_1_MMIO_SIZE   = 0x00004000,

		EPIT_1_IRQ         = 40,
		EPIT_1_MMIO_BASE   = 0x53fac000,
		EPIT_1_MMIO_SIZE   = 0x00004000,

		EPIT_2_IRQ         = 41,
		EPIT_2_MMIO_BASE   = 0x53fb0000,
		EPIT_2_MMIO_SIZE   = 0x00004000,

		GPIO1_MMIO_BASE    = 0x53f84000,
		GPIO1_MMIO_SIZE    = 0x4000,
		GPIO2_MMIO_BASE    = 0x53f88000,
		GPIO2_MMIO_SIZE    = 0x4000,
		GPIO3_MMIO_BASE    = 0x53f8c000,
		GPIO3_MMIO_SIZE    = 0x4000,
		GPIO4_MMIO_BASE    = 0x53f90000,
		GPIO4_MMIO_SIZE    = 0x4000,
		GPIO5_MMIO_BASE    = 0x53fdc000,
		GPIO5_MMIO_SIZE    = 0x4000,
		GPIO6_MMIO_BASE    = 0x53fe0000,
		GPIO6_MMIO_SIZE    = 0x4000,
		GPIO7_MMIO_BASE    = 0x53fe4000,
		GPIO7_MMIO_SIZE    = 0x4000,
		GPIO1_IRQL         = 50,
		GPIO1_IRQH         = 51,
		GPIO2_IRQL         = 52,
		GPIO2_IRQH         = 53,
		GPIO3_IRQL         = 54,
		GPIO3_IRQH         = 55,
		GPIO4_IRQL         = 56,
		GPIO4_IRQH         = 57,
		GPIO5_IRQL         = 103,
		GPIO5_IRQH         = 104,
		GPIO6_IRQL         = 105,
		GPIO6_IRQH         = 106,
		GPIO7_IRQL         = 107,
		GPIO7_IRQH         = 108,

		IRQ_CONTROLLER_BASE = 0x0fffc000,
		IRQ_CONTROLLER_SIZE = 0x00004000,

		AIPS_1_MMIO_BASE   = 0x53f00000,
		AIPS_2_MMIO_BASE   = 0x63f00000,

		IOMUXC_BASE        = 0x53fa8000,
		IOMUXC_SIZE        = 0x00004000,

		PWM2_BASE          = 0x53fb8000,
		PWM2_SIZE          = 0x00004000,

		IPU_BASE           = 0x18000000,
		IPU_SIZE           = 0x08000000,

		SRC_BASE           = 0x53fd0000,
		SRC_SIZE           = 0x00004000,

		CCM_BASE           = 0x53FD4000,
		CCM_SIZE           = 0x00004000,

		I2C_2_IRQ          = 63,
		I2C_2_BASE         = 0x63fc4000,
		I2C_2_SIZE         = 0x00004000,

		I2C_3_IRQ          = 64,
		I2C_3_BASE         = 0x53fec000,
		I2C_3_SIZE         = 0x00004000,

		IIM_BASE           = 0x63f98000,
		IIM_SIZE           = 0x00004000,

		CSU_BASE           = 0x63f9c000,
		CSU_SIZE           = 0x00001000,

		M4IF_BASE          = 0x63fd8000,
		M4IF_SIZE          = 0x00001000,

		/* wether board provides security extension */
		SECURITY_EXTENSION = 1,

		/* CPU cache */
		CACHE_LINE_SIZE_LOG2 = 6,
	};
};

#endif /* _INCLUDE__SPEC__IMX53__DRIVERS__BOARD_BASE_SUPPORT_H_ */
