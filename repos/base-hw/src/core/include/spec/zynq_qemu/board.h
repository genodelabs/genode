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

#ifndef _CORE__INCLUDE__SPEC__ZYNQ_QEMU__BOARD_H_
#define _CORE__INCLUDE__SPEC__ZYNQ_QEMU__BOARD_H_

/* core includes */
#include <spec/cortex_a9/board_support.h>

namespace Genode
{
	struct Board : Cortex_a9::Board
	{
		enum {
			KERNEL_UART_BASE = UART_0_MMIO_BASE,
			KERNEL_UART_SIZE = UART_SIZE,
		};
	};
}

#endif /* _CORE__INCLUDE__SPEC__ZYNQ_QEMU__BOARD_H_ */
