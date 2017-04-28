/*
 * \brief  MMIO and IRQ definitions common to Exynos5 SoC
 * \author Stefan Kalkowski
 * \date   2013-11-25
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__EXYNOS5_H_
#define _INCLUDE__DRIVERS__DEFS__EXYNOS5_H_

namespace Exynos5 {
	enum {
		/* normal RAM */
		RAM_0_BASE = 0x40000000,
		RAM_0_SIZE = 0x80000000,

		/* device IO memory */
		MMIO_0_BASE = 0x10000000,
		MMIO_0_SIZE = 0x10000000,

		/* interrupt controller */
		IRQ_CONTROLLER_BASE         = 0x10480000,
		IRQ_CONTROLLER_SIZE         = 0x00010000,
		IRQ_CONTROLLER_VT_CTRL_BASE = 0x10484000,
		IRQ_CONTROLLER_VT_CTRL_SIZE = 0x1000,
		IRQ_CONTROLLER_VT_CPU_BASE  = 0x10486000,
		IRQ_CONTROLLER_VT_CPU_SIZE  = 0x1000,

		/* virtual interrupts */
		VT_MAINTAINANCE_IRQ = 25,
		VT_TIMER_IRQ        = 27,

		/* UART */
		UART_2_MMIO_BASE = 0x12C20000,
		UART_2_MMIO_SIZE = 0x1000,
		UART_2_IRQ       = 85,

		/* pulse-width-modulation timer  */
		PWM_MMIO_BASE = 0x12dd0000,
		PWM_MMIO_SIZE = 0x1000,
		PWM_CLOCK     = 66000000,
		PWM_IRQ_0     = 68,

		/* multicore timer */
		MCT_MMIO_BASE = 0x101c0000,
		MCT_MMIO_SIZE = 0x1000,
		MCT_CLOCK     = 24000000,
		MCT_IRQ_L0    = 152,
		MCT_IRQ_L1    = 153,

		/* CPU cache */
		CACHE_LINE_SIZE_LOG2 = 6,

		/* IRAM */
		IRAM_BASE = 0x02020000,

		/* hardware name of the primary processor */
		PRIMARY_MPIDR_AFF_0 = 0,

		/* SATA/AHCI */
		SATA_IRQ = 147,

		/* SD card */
		SDMMC0_IRQ = 107,


		/******************************
		 ** HDMI memory map and irqs **
		 ******************************/

		/* Mixer base */
		MIXER_BASE = 0x14450000,

		/* HDMI base */
		HDMI_BASE = 0x14530000,

		/* I2C BASE */
		I2C_BASE = 0x12ce0000,

		/* I2C */
		I2C_HDMI_IRQ = 96,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__EXYNOS5_H_ */
