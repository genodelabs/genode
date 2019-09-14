/*
 * \brief  Board definitions for Qemu ARM virt machine
 * \author Piotr Tworek
 * \date   2019-09-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <drivers/uart/pl011.h>
#include <hw/spec/arm/boot_info.h>

namespace Hw::Arm_virt_board {
	using Serial   = Genode::Pl011_uart;

	enum {
		RAM_BASE   = 0x40000000,
		RAM_SIZE   = 0x80000000,

		UART_BASE  = 0x09000000,
		UART_SIZE  = 0x1000,
		UART_CLOCK = 250000000,

		CACHE_LINE_SIZE_LOG2 = 6,
	};

	namespace Cpu_mmio {
		enum {
			IRQ_CONTROLLER_DISTR_BASE  = 0x08000000,
			IRQ_CONTROLLER_DISTR_SIZE  = 0x10000,
			IRQ_CONTROLLER_CPU_BASE    = 0x08010000,
			IRQ_CONTROLLER_CPU_SIZE    = 0x10000,
			IRQ_CONTROLLER_REDIST_BASE = 0x080A0000,
			IRQ_CONTROLLER_REDIST_SIZE = 0xC0000,
		};
	};
};
