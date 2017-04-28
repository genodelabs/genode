/*
 * \brief   Odroid XU specific board definitions
 * \author  Stefan Kalkowski
 * \date    2017-04-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ODROID_XU__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__ODROID_XU__BOARD_H_

#include <drivers/defs/odroid_xu.h>
#include <drivers/uart/exynos.h>

#include <hw/spec/arm/cortex_a15.h>
#include <hw/spec/arm/lpae.h>
#include <spec/arm/cpu.h>
#include <spec/arm/pic.h>

namespace Board {

	using namespace Odroid_xu;
	using Cpu_mmio = Hw::Cortex_a15_mmio<IRQ_CONTROLLER_BASE>;
	using Serial   = Genode::Exynos_uart;

	enum {
		UART_BASE  = UART_2_MMIO_BASE,
		UART_CLOCK = UART_2_CLOCK,
	};
}

#endif /* _SRC__BOOTSTRAP__SPEC__ODROID_XU__BOARD_H_ */
