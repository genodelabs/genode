/*
 * \brief   Arndale specific board definitions
 * \author  Stefan Kalkowski
 * \date    2017-04-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ARNDALE__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__ARNDALE__BOARD_H_

#include <drivers/board_base.h>
#include <drivers/uart_base.h>

#include <hw/spec/arm/cortex_a15.h>
#include <hw/spec/arm/lpae.h>
#include <spec/arm/cpu.h>
#include <spec/arm/pic.h>

namespace Bootstrap {
	class L2_cache;

	using Serial = Genode::Exynos_uart_base;

	enum {
		UART_BASE     = Genode::Board_base::UART_2_MMIO_BASE,
		UART_CLOCK    = Genode::Board_base::UART_2_CLOCK,
		CPU_MMIO_BASE = Genode::Board_base::IRQ_CONTROLLER_BASE,
	};
}

#endif /* _SRC__BOOTSTRAP__SPEC__ARNDALE__BOARD_H_ */
