/*
 * \brief   RISC-V Qemu specific board definitions
 * \author  Sebastian Sumpf
 * \author  Stefan Kalkowski
 * \date    2021-02-09
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__RISCV__QEMU_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__RISCV__QEMU_BOARD_H_

#include <hw/spec/riscv/boot_info.h>
#include <hw/spec/riscv/page_table.h>
#include <hw/spec/riscv/sbi.h>

namespace Hw::Riscv_board {

	enum {
		RAM_BASE = 0x80020000,
		RAM_SIZE = 0x7fe0000,
		TIMER_HZ = 10000000,
	};

	enum { UART_BASE, UART_CLOCK };

	struct Serial : Hw::Riscv_uart
	{
		Serial(Genode::addr_t, Genode::size_t, unsigned) { }
	};
}

#endif /* _SRC__INCLUDE__HW__SPEC__RISCV__QEMU_BOARD_H_ */
