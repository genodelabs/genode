/*
 * \brief  Definitions for the Imx7 dual sabre board
 * \author Stefan Kalkowski
 * \date   2018-10-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__IMX7D_SABRE_H_
#define _INCLUDE__DRIVERS__DEFS__IMX7D_SABRE_H_

#include <drivers/defs/arm_v7.h>

namespace Imx7d_sabre {

	using namespace Arm_v7;

	enum {
		RAM_0_BASE                  = 0x80000000UL,
		RAM_0_SIZE                  = 0x40000000UL,

		IRQ_CONTROLLER_BASE         = 0x31000000UL,
		IRQ_CONTROLLER_SIZE         = 0x8000,

		SRC_MMIO_BASE               = 0x30390000UL,

		AIPS_1_MMIO_BASE            = 0x301f0000UL,
		AIPS_2_MMIO_BASE            = 0x305f0000UL,
		AIPS_3_MMIO_BASE            = 0x309f0000UL,

		UART_1_MMIO_BASE            = 0x30860000UL,
		UART_1_MMIO_SIZE            = 0x10000UL,

		TIMER_CLOCK                 = 1000000000UL,
	};
}

#endif /* _INCLUDE__DRIVERS__DEFS__IMX7D_SABRE_H_ */
