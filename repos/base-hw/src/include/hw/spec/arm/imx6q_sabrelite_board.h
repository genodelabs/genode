/*
 * \brief   i.MX6Quad Sabrelite specific board definitions
 * \author  Stefan Kalkowski
 * \date    2019-05-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__IMX6Q_SABRELITE_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__IMX6Q_SABRELITE_BOARD_H_

#include <drivers/defs/imx6q_sabrelite.h>
#include <drivers/uart/imx.h>
#include <hw/spec/arm/boot_info.h>
#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

namespace Hw::Imx6q_sabrelite_board {

	using namespace Imx6q_sabrelite;

	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using Serial   = Genode::Imx_uart;

	enum {
		UART_BASE  = UART_2_MMIO_BASE,
		UART_SIZE  = UART_2_MMIO_SIZE,
		UART_CLOCK = 0, /* dummy value, not used */
	};
}

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__IMX6Q_SABRELITE_BOARD_H_ */
