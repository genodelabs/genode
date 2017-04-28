/*
 * \brief  MMIO and IRQ definitions for the InSignal Arndale 5 board
 * \author Martin stein
 * \date   2013-01-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__ARNDALE_H_
#define _INCLUDE__DRIVERS__DEFS__ARNDALE_H_

/* Genode includes */
#include <drivers/defs/exynos5.h>

namespace Arndale {

	using namespace Exynos5;

	enum
	{
		/* clock management unit */
		CMU_MMIO_BASE = 0x10010000,
		CMU_MMIO_SIZE = 0x24000,

		/* power management unit */
		PMU_MMIO_BASE = 0x10040000,
		PMU_MMIO_SIZE = 0x5000,

		/* USB */
		USB_HOST20_IRQ = 103,
		USB_DRD30_IRQ  = 104,

		/* UART */
		UART_2_CLOCK = 100000000,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__ARNDALE_H_ */
