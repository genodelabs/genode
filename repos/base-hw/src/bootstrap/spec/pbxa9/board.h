/*
 * \brief   Pbxa9 specific board definitions
 * \author  Stefan Kalkowski
 * \date    2017-02-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__PBXA9__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__PBXA9__BOARD_H_

#include <drivers/defs/pbxa9.h>
#include <drivers/uart/pl011.h>
#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

#include <spec/arm/cortex_a9_actlr.h>
#include <spec/arm/cortex_a9_page_table.h>
#include <spec/arm/cpu.h>
#include <spec/arm/pic.h>

namespace Board {

	using namespace Pbxa9;

	using L2_cache = Hw::Pl310;
	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using Serial   = Genode::Pl011_uart;

	enum {
		UART_BASE  = PL011_0_MMIO_BASE,
		UART_CLOCK = PL011_0_CLOCK,
	};
}

#endif /* _SRC__BOOTSTRAP__SPEC__PBXA9__BOARD_H_ */
