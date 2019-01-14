/*
 * \brief  Board driver
 * \author Stefan Kalkowski
 * \date   2019-01-05
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__IMX6Q_SABRELITE__BOARD_H_
#define _CORE__SPEC__IMX6Q_SABRELITE__BOARD_H_

/* base includes */
#include <drivers/defs/imx6q_sabrelite.h>
#include <drivers/uart/imx.h>

#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

namespace Board {
	using namespace Imx6q_sabrelite;
	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using L2_cache = Hw::Pl310;
	using Serial   = Genode::Imx_uart;

	enum {
		UART_BASE  = UART_2_MMIO_BASE,
		UART_CLOCK = 0, /* dummy value, not used */
	};

	static constexpr bool SMP = true;

	L2_cache & l2_cache();
}

#endif /* _CORE__SPEC__WAND_QUAD__BOARD_H_ */
