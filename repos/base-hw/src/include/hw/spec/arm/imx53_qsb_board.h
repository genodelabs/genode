/*
 * \brief   i.MX53 Quickstart board definitions
 * \author  Stefan Kalkowski
 * \date    2019-05-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__IMX53_QSB_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__IMX53_QSB_BOARD_H_

#include <drivers/defs/imx53_qsb.h>
#include <drivers/uart/imx.h>
#include <hw/spec/arm/boot_info.h>

namespace Hw::Imx53_qsb_board {
	using namespace Imx53_qsb;
	using Serial = Genode::Imx_uart;

	enum {
		UART_BASE  = UART_1_MMIO_BASE,
		UART_CLOCK = 0, /* ignored value */
	};
}

#endif /* _SRC__INCLUDE__HW__SPEC__IMX53_QSB_BOARD_H_ */
