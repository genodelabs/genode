/*
 * \brief   Riscv spike specific board definitions
 * \author  Stefan Kalkowski
 * \date    2019-05-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__RISCV__BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__RISCV__BOARD_H_

#include <hw/spec/riscv/boot_info.h>
#include <hw/spec/riscv/page_table.h>
#include <hw/spec/riscv/uart.h>
#include <drivers/defs/riscv.h>

namespace Hw::Riscv_board {

	using namespace Riscv;

	enum { UART_BASE, UART_CLOCK };
	struct Serial : Hw::Riscv_uart {
		Serial(Genode::addr_t, Genode::size_t, unsigned) {} };
}

#endif /* _SRC__INCLUDE__HW__SPEC__RISCV__BOARD_H_ */
