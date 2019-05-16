/*
 * \brief   Arndale specific board definitions
 * \author  Stefan Kalkowski
 * \date    2019-05-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__ARNDALE_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__ARNDALE_BOARD_H_

#include <drivers/defs/arndale.h>
#include <drivers/uart/exynos.h>
#include <hw/spec/arm/boot_info.h>
#include <hw/spec/arm/cortex_a15.h>

namespace Hw::Arndale_board {

	using namespace Arndale;
	using Cpu_mmio = Hw::Cortex_a15_mmio<IRQ_CONTROLLER_BASE>;
	using Serial = Genode::Exynos_uart;

	enum {
		UART_BASE  = UART_2_MMIO_BASE,
		UART_CLOCK = UART_2_CLOCK,
	};
}

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__ARNDALE_BOARD_H_ */
