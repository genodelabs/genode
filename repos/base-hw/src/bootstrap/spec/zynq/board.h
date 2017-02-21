/*
 * \brief   Zynq specific board definitions
 * \author  Stefan Kalkowski
 * \date    2017-02-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__ZYNQ__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__ZYNQ__BOARD_H_

#include <drivers/board_base.h>
#include <drivers/uart_base.h>
#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

#include <spec/arm/cortex_a9_actlr.h>
#include <spec/arm/cortex_a9_page_table.h>
#include <spec/arm/cpu.h>
#include <spec/arm/pic.h>

namespace Bootstrap {
	using L2_cache = Hw::Pl310;
	using Serial   = Genode::Xilinx_uartps_base;

	enum {
		UART_BASE  = Genode::Board_base::UART_0_MMIO_BASE,
		UART_CLOCK = Genode::Board_base::UART_CLOCK,
		CPU_MMIO_BASE = Genode::Board_base::CORTEX_A9_PRIVATE_MEM_BASE,
	};
}

#endif /* _SRC__BOOTSTRAP__SPEC__ZYNQ__BOARD_H_ */
