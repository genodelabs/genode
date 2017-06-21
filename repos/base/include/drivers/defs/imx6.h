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
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__IMX6_H_ */
