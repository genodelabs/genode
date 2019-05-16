/*
 * \brief   Nit6 SOLOX specific board definitions
 * \author  Stefan Kalkowski
 * \date    2019-05-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__NIT6_SOLOX_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__NIT6_SOLOX_BOARD_H_

#include <drivers/defs/nit6_solox.h>
#include <drivers/uart/imx.h>
#include <hw/spec/arm/boot_info.h>
#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

namespace Hw::Nit6_solox_board {

	using namespace Nit6_solox;

	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using Serial   = Genode::Imx_uart;

	enum {
		UART_BASE  = UART_1_MMIO_BASE,
		UART_SIZE  = UART_1_MMIO_SIZE,
		UART_CLOCK = 0, /* dummy value, not used */
	};
}

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__NIT6_SOLOX_BOARD_H_ */
