/*
 * \brief  Board definitions for the i.MX53
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__IMX53__DRIVERS__BOARD_BASE_H_
#define _INCLUDE__PLATFORM__IMX53__DRIVERS__BOARD_BASE_H_

namespace Genode
{
	/**
	 * i.MX53 motherboard
	 */
	struct Board_base
	{
		enum {
			MMIO_BASE          = 0x0,
			MMIO_SIZE          = 0x70000000,

			RAM_BASE           = 0x70000000,
			RAM_SIZE           = 0x40000000,

			UART_1_IRQ         = 31,
			UART_1_MMIO_BASE   = 0x53fbc000,
			UART_1_MMIO_SIZE   = 0x00004000,

			EPIT_1_IRQ         = 40,
			EPIT_1_MMIO_BASE   = 0x53fac000,
			EPIT_1_MMIO_SIZE   = 0x00004000,

			EPIT_2_IRQ         = 41,
			EPIT_2_MMIO_BASE   = 0x53fb0000,
			EPIT_2_MMIO_SIZE   = 0x00004000,

			TZIC_MMIO_BASE     = 0x0fffc000,
			TZIC_MMIO_SIZE     = 0x00004000,

			AIPS_1_MMIO_BASE   = 0x53f00000,
			AIPS_2_MMIO_BASE   = 0x63f00000,

			IPU_ERR_IRQ        = 10,
			IPU_SYNC_IRQ       = 11,
			IPU_BASE           = 0x18000000,
			IPU_SIZE           = 0x08000000,

			SRC_BASE           = 0x53fd0000,
			SRC_SIZE           = 0x00004000,

			CCM_BASE           = 0x53FD4000,
			CCM_SIZE           = 0x00004000,

			SECURITY_EXTENSION = 1,
		};
	};
}

#endif /* _INCLUDE__PLATFORM__IMX53__DRIVERS__BOARD_BASE_H_ */

