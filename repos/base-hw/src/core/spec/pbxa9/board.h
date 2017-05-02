/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__PBXA9__BOARD_H_
#define _CORE__SPEC__PBXA9__BOARD_H_

/* base includes */
#include <drivers/defs/pbxa9.h>
#include <drivers/uart/pl011.h>

#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

namespace Board {
	using namespace Pbxa9;

	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using L2_cache = Hw::Pl310;
	using Serial   = Genode::Pl011_uart;

	enum {
		UART_BASE  = PL011_0_MMIO_BASE,
		UART_CLOCK = PL011_0_CLOCK,
	};

	static constexpr bool SMP = true;

	L2_cache & l2_cache();
}

#endif /* _CORE__SPEC__PBXA9__BOARD_H_ */
