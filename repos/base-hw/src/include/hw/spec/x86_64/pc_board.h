/*
 * \brief   PC specific board definitions
 * \author  Stefan Kalkowski
 * \date    2019-05-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__X86_64__PC_BOARD_H_
#define _SRC__INCLUDE__HW__SPEC__X86_64__PC_BOARD_H_

#include <drivers/uart/x86_pc.h>

namespace Hw::Pc_board {
	struct Serial;
	enum Dummies { UART_BASE, UART_CLOCK };
}


struct Hw::Pc_board::Serial : Genode::X86_uart
{
	Serial(Genode::addr_t, Genode::size_t, unsigned);
};

#endif /* _SRC__BOOTSTRAP__SPEC__X86_64__BOARD_H_ */
