/*
 * \brief  x86_64 constants
 * \author Reto Buerki
 * \date   2015-03-18
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__BOARD_H_
#define _CORE__SPEC__X86_64__BOARD_H_

#include <drivers/uart/x86_pc.h>

namespace Board {
	struct Serial;
	enum Dummies { UART_BASE, UART_CLOCK };

	enum {
		VECTOR_REMAP_BASE   = 48,
		TIMER_VECTOR_KERNEL = 32,
		TIMER_VECTOR_USER   = 50,
		ISA_IRQ_END         = 15,
	};
}


struct Board::Serial : Genode::X86_uart {
	Serial(Genode::addr_t, Genode::size_t, unsigned);
};

#endif /* _CORE__SPEC__X86_64__BOARD_H_ */
