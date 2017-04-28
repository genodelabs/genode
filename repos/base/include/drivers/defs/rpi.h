/*
 * \brief  MMIO and IRQ definitions for the Raspberry Pi
 * \author Norman Feske
 * \date   2013-04-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__RPI_H_
#define _INCLUDE__DRIVERS__DEFS__RPI_H_

/* Genode includes */
#include <util/mmio.h>

namespace Rpi {
	enum {
		RAM_0_BASE = 0x00000000,
		RAM_0_SIZE = 0x10000000, /* XXX ? */

		MMIO_0_BASE = 0x20000000,
		MMIO_0_SIZE = 0x02000000,

		/*
		 * IRQ numbers   0..7 refer to the basic IRQs.
		 * IRQ numbers  8..39 refer to GPU IRQs  0..31.
		 * IRQ numbers 40..71 refer to GPU IRQs 32..63.
		 */
		GPU_IRQ_BASE = 8,

		SYSTEM_TIMER_IRQ       = GPU_IRQ_BASE + 1,
		SYSTEM_TIMER_MMIO_BASE = 0x20003000,
		SYSTEM_TIMER_MMIO_SIZE = 0x1000,
		SYSTEM_TIMER_CLOCK     = 1000000,

		PL011_0_IRQ       = 57,
		PL011_0_MMIO_BASE = 0x20201000,
		PL011_0_MMIO_SIZE = 0x1000,
		PL011_0_CLOCK     = 3000000,

		IRQ_CONTROLLER_BASE = 0x2000b200,
		IRQ_CONTROLLER_SIZE = 0x100,

		GPIO_CONTROLLER_BASE = 0x20200000,
		GPIO_CONTROLLER_SIZE = 0x1000,

		USB_DWC_OTG_BASE = 0x20980000,
		USB_DWC_OTG_SIZE = 0x20000,

		/* timer */
		TIMER_IRQ = 0,

		/* USB host controller */
		DWC_IRQ = 17,

		/* CPU cache */
		CACHE_LINE_SIZE_LOG2 = 5,

		/* SD card */
		SDHCI_BASE = MMIO_0_BASE + 0x300000,
		SDHCI_SIZE = 0x100,
		SDHCI_IRQ  = 62,
	};

	enum Videocore_cache_policy { NON_COHERENT = 0,
	                              COHERENT     = 1,
	                              L2_ONLY      = 2,
	                              UNCACHED     = 3 };
};

#endif /* _INCLUDE__DRIVERS__DEFS__RPI_H_ */
