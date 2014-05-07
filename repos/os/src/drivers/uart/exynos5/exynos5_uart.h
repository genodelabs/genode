/*
 * \brief  Driver for EXYNOS5 UARTs
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2013-06-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EXYNOS_UART_H_
#define _EXYNOS_UART_H_

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <os/irq_activation.h>
#include <os/attached_io_mem_dataspace.h>

#include <drivers/uart/exynos_uart_base.h>

/* local includes */
#include "uart_driver.h"

class Exynos_uart : public Genode::Exynos_uart_base,
                    public Uart::Driver,
                    public Genode::Irq_handler
{
	public:

		/**
		 * Constructor
		 */
		Exynos_uart(Genode::Attached_io_mem_dataspace *uart_mmio, int irq_number,
		          unsigned baud_rate, Uart::Char_avail_callback &callback)
		:
			Exynos_uart_base((Genode::addr_t)uart_mmio->local_addr<void>(),
			                 Genode::Board_base::UART_2_CLOCK, baud_rate) { }

		/**
		 * * IRQ handler interface **
		 */
		void handle_irq(int irq_number) { }

		/**
		 * * UART driver interface **
		 */
		void put_char(char c) { Exynos_uart_base::put_char(c); }

		bool char_avail() { return false; }

		char get_char() { return 0; }

		void baud_rate(int bits_per_second) {}
};

#endif /* _EXYNOS_UART_H_ */
