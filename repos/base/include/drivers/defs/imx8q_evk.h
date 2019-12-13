/*
 * \brief  MMIO and IRQ definitions for the i.MX8Q EVK board
 * \author Christian Prochaska
 * \date   2019-09-26
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__IMX8Q_EVK_H_
#define _INCLUDE__DRIVERS__DEFS__IMX8Q_EVK_H_

namespace Imx8 {
	enum {
		/* SD host controller */
		SDHC_2_IRQ       = 55,
		SDHC_2_MMIO_BASE = 0x30b50000,
		SDHC_2_MMIO_SIZE = 0x00010000,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__IMX8Q_EVK_H_ */
