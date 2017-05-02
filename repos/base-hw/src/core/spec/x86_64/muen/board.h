/*
 * \brief  x86_64_muen constants
 * \author Adrian-Ken Rueegsegger
 * \date   2015-07-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__MUEN__BOARD_H_
#define _CORE__SPEC__X86_64__MUEN__BOARD_H_

#include <drivers/uart/x86_pc.h>

namespace Board {
	struct Serial;
	enum Dummies { UART_BASE, UART_CLOCK };

	enum {
		TIMER_BASE_ADDR         = 0xe00010000,
		TIMER_SIZE              = 0x1000,
		TIMER_PREEMPT_BASE_ADDR = 0xe00011000,
		TIMER_PREEMPT_SIZE      = 0x1000,

		VECTOR_REMAP_BASE   = 48,
		TIMER_EVENT_PREEMPT = 30,
		TIMER_EVENT_KERNEL  = 31,
		TIMER_VECTOR_KERNEL = 32,
		TIMER_VECTOR_USER   = 50,
	};
}


struct Board::Serial : Genode::X86_uart {
	Serial(Genode::addr_t, Genode::size_t, unsigned);
};

#endif /* _CORE__SPEC__X86_64__MUEN__BOARD_H_ */
