/*
 * \brief   i.MX53 Quickstart board definitions
 * \author  Stefan Kalkowski
 * \date    2017-03-22
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__USB_ARMORY__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__USB_ARMORY__BOARD_H_

#include <drivers/defs/usb_armory.h>
#include <drivers/uart/imx.h>
#include <hw/spec/arm/imx_tzic.h>

#include <spec/arm/cortex_a8_page_table.h>
#include <spec/arm/cpu.h>

namespace Bootstrap { using Hw::Pic; }

namespace Board {
	using namespace Usb_armory;
	using Serial = Genode::Imx_uart;

	enum {
		UART_BASE  = UART_1_MMIO_BASE,
		UART_CLOCK = 0, /* ignored value */
	};

	bool secure_irq(unsigned irq);
}

#endif /* _SRC__BOOTSTRAP__SPEC__USB_ARMORY__BOARD_H_ */
