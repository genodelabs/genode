/*
 * \brief   PC specific board definitions
 * \author  Stefan Kalkowski
 * \date    2017-04-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__X86_64__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__X86_64__BOARD_H_

#include <drivers/uart_base.h>

#include <hw/spec/x86_64/page_table.h>
#include <hw/spec/x86_64/cpu.h>
#include <hw/spec/x86_64/x86_64.h>

namespace Genode { struct Board_base {}; }

namespace Bootstrap {

	struct Serial;
	struct Pic {};

	using Cpu = Hw::X86_64_cpu;

	enum Dummies { UART_BASE, UART_CLOCK };
}


struct Bootstrap::Serial : Genode::X86_uart_base
{
	Serial(Genode::addr_t, Genode::size_t, unsigned);
};

#endif /* _SRC__BOOTSTRAP__SPEC__X86_64__BOARD_H_ */
