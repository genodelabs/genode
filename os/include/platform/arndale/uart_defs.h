/*
 * \brief  EXYNOS5 UART definitions
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__ARNDALE__UART_DEFS_H_
#define _INCLUDE__PLATFORM__ARNDALE__UART_DEFS_H_

#include <platform/arndale/drivers/board_base.h>

enum {
	/** Number of UARTs */
	UARTS_NUM = 2,

	BAUD_115200 = 115200,
};


static struct Exynos_uart_cfg {
	Genode::addr_t mmio_base;
	Genode::size_t mmio_size;
	int            irq_number;
} exynos_uart_cfg[UARTS_NUM] = {
	/* temporary workaround having first UART twice (most run-scripts have first UART reserved for the kernel ) */
	{ Genode::Board_base::UART_2_MMIO_BASE, 4096, Genode::Board_base::UART_2_IRQ },
	{ Genode::Board_base::UART_2_MMIO_BASE, 4096, Genode::Board_base::UART_2_IRQ },
};

#endif /* _INCLUDE__PLATFORM__ARNDALE__UART_DEFS_H_ */
