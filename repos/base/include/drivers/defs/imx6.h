/*
 * \brief  MMIO and IRQ definitions common to i.MX6 SoC
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Josef Soentgen
 * \author Martin Stein
 * \date   2017-06-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__IMX6_H_
#define _INCLUDE__DRIVERS__DEFS__IMX6_H_

namespace Imx6 {
	enum {
		/* SD host controller */
		SDHC_IRQ       = 54,
		SDHC_MMIO_BASE = 0x02190000,
		SDHC_MMIO_SIZE = 0x00004000,

		/* GPIO */
		GPIO1_MMIO_BASE    = 0x0209c000,
		GPIO1_MMIO_SIZE    = 0x4000,
		GPIO2_MMIO_BASE    = 0x020a0000,
		GPIO2_MMIO_SIZE    = 0x4000,
		GPIO3_MMIO_BASE    = 0x020a4000,
		GPIO3_MMIO_SIZE    = 0x4000,
		GPIO4_MMIO_BASE    = 0x020a8000,
		GPIO4_MMIO_SIZE    = 0x4000,
		GPIO5_MMIO_BASE    = 0x020ac000,
		GPIO5_MMIO_SIZE    = 0x4000,
		GPIO6_MMIO_BASE    = 0x020b0000,
		GPIO6_MMIO_SIZE    = 0x4000,
		GPIO7_MMIO_BASE    = 0x020b4000,
		GPIO7_MMIO_SIZE    = 0x4000,
		GPIO1_IRQL         = 98,
		GPIO1_IRQH         = 99,
		GPIO2_IRQL         = 100,
		GPIO2_IRQH         = 101,
		GPIO3_IRQL         = 102,
		GPIO3_IRQH         = 103,
		GPIO4_IRQL         = 104,
		GPIO4_IRQH         = 105,
		GPIO5_IRQL         = 106,
		GPIO5_IRQH         = 107,
		GPIO6_IRQL         = 108,
		GPIO6_IRQH         = 109,
		GPIO7_IRQL         = 110,
		GPIO7_IRQH         = 111,

	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__IMX6_H_ */
