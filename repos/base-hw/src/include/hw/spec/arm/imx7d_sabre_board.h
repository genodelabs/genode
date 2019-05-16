/*
 * \brief   Imx7 Sabrelite specific board definitions
 * \author  Stefan Kalkowski
 * \date    2018-11-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__IMX7_SABRELITE_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__IMX7_SABRELITE_BOARD_H_

#include <drivers/defs/imx7d_sabre.h>
#include <drivers/uart/imx.h>
#include <hw/spec/arm/boot_info.h>
#include <hw/spec/arm/cortex_a15.h>

namespace Hw::Imx7d_sabre_board {

	using namespace Imx7d_sabre;

	using Cpu_mmio = Hw::Cortex_a15_mmio<IRQ_CONTROLLER_BASE>;
	using Serial = Genode::Imx_uart;

	enum {
		UART_BASE  = UART_1_MMIO_BASE,
		UART_CLOCK = 0, /* unsued value */
	};
}

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__IMX7_SABRELITE_BOARD_H_ */
