/*
 * \brief  Board driver for core on Zynq
 * \author Johannes Schlatow
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ZYNQ_QEMU__BOARD_H_
#define _CORE__SPEC__ZYNQ_QEMU__BOARD_H_

/* base includes */
#include <drivers/defs/zynq_qemu.h>
#include <drivers/uart/xilinx.h>

#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

namespace Board {
	using namespace Zynq_qemu;
	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using L2_cache = Hw::Pl310;
	using Serial   = Genode::Xilinx_uart;

	enum {
		UART_BASE  = UART_0_MMIO_BASE,
	};

	static constexpr bool SMP = true;

	L2_cache & l2_cache();
}

#endif /* _CORE__SPEC__ZYNQ_QEMU__BOARD_H_ */
