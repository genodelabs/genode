/*
 * \brief  Board driver
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__USB_ARMORY__BOARD_H_
#define _CORE__SPEC__USB_ARMORY__BOARD_H_

#include <drivers/defs/usb_armory.h>
#include <drivers/uart/imx.h>

namespace Board {
	using namespace Usb_armory;
	using Serial   = Genode::Imx_uart;

	enum {
		UART_BASE  = UART_1_MMIO_BASE,
		UART_CLOCK = 0, /* dummy value, not used */
	};

	static constexpr bool SMP = false;
}

#endif /* _CORE__SPEC__USB_ARMORY__BOARD_H_ */
