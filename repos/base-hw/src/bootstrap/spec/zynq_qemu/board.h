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

#include <drivers/defs/zynq_qemu.h>
#include <drivers/uart/xilinx.h>
#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

#include <spec/arm/cortex_a9_actlr.h>
#include <spec/arm/cortex_a9_page_table.h>
#include <spec/arm/cpu.h>
#include <spec/arm/pic.h>

namespace Board {
	using namespace Zynq_qemu;
	using L2_cache = Hw::Pl310;
	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using Serial   = Genode::Xilinx_uart;

	enum {
		UART_BASE  = UART_0_MMIO_BASE,
	};
}

#endif /* _SRC__BOOTSTRAP__SPEC__ZYNQ__BOARD_H_ */
