/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2018-11-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__IMX7D_SABRE__BOARD_H_
#define _CORE__SPEC__IMX7D_SABRE__BOARD_H_

/* base includes */
#include <drivers/defs/imx7d_sabre.h>
#include <drivers/uart/imx.h>

#include <hw/spec/arm/cortex_a15.h>

namespace Board {
	using namespace Imx7d_sabre;
	using Cpu_mmio = Hw::Cortex_a15_mmio<IRQ_CONTROLLER_BASE>;
	using Serial = Genode::Imx_uart;

	enum {
		UART_BASE  = UART_1_MMIO_BASE,
		UART_CLOCK = 0, /* unused value */
	};

	static constexpr bool SMP = true;
}

#endif /* _CORE__SPEC__IMX7_SABRELITE__BOARD_H_ */
