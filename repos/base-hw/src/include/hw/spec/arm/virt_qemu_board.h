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

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__VIRT_QEMU_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__VIRT_QEMU_BOARD_H_

#include <drivers/uart/pl011.h>
#include <hw/spec/arm/boot_info.h>

namespace Hw::Virt_qemu_board {
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
			IRQ_CONTROLLER_DISTR_BASE   = 0x08000000,
			IRQ_CONTROLLER_DISTR_SIZE   = 0x10000,
			IRQ_CONTROLLER_CPU_BASE     = 0x08010000,
			IRQ_CONTROLLER_CPU_SIZE     = 0x10000,
			IRQ_CONTROLLER_VT_CTRL_BASE = 0x8030000,
			IRQ_CONTROLLER_VT_CTRL_SIZE = 0x1000,
			IRQ_CONTROLLER_VT_CPU_BASE  = 0x8040000,
			IRQ_CONTROLLER_VT_CPU_SIZE  = 0x1000,
			IRQ_CONTROLLER_REDIST_BASE  = 0x080A0000,
			IRQ_CONTROLLER_REDIST_SIZE  = 0xC0000,
		};
	};
};

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__VIRT_QEMU_BOARD_H_ */
