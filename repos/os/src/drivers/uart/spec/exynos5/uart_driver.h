/*
 * \brief  Driver for EXYNOS5 UARTs
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2013-06-05
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UART_DRIVER_H_
#define _UART_DRIVER_H_

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/env.h>
#include <drivers/uart_base.h>
#include <drivers/board_base.h>

enum { UARTS_NUM = 2 }; /* needed by base class definitions */

/* local includes */
#include <uart_driver_base.h>


class Uart::Driver : public Genode::Attached_io_mem_dataspace,
                     public Genode::Exynos_uart_base,
                     public Uart::Driver_base
{
	private:

		enum { BAUD_115200 = 115200 };

		struct Uart {
			Genode::addr_t mmio_base;
			Genode::size_t mmio_size;
			int            irq_number;
		};

		Uart & _config(unsigned index)
		{
			using namespace Genode;

			static Uart cfg[UARTS_NUM] = {
				/*
				 * temporary workaround having first UART twice
				 * (most run-scripts have first UART reserved for the kernel)
				 */
				{ Board_base::UART_2_MMIO_BASE, 4096, Board_base::UART_2_IRQ },
				{ Board_base::UART_2_MMIO_BASE, 4096, Board_base::UART_2_IRQ },
			};
			return cfg[index];
		}

		unsigned _baud_rate(unsigned baud_rate)
		{
			if (baud_rate != BAUD_115200)
				Genode::warning("baud_rate ", baud_rate,
				                " not supported, set to default");
			return BAUD_115200;
		}

	public:

		Driver(Genode::Env &env, unsigned index,
		       unsigned baud_rate, Char_avail_functor &func)
		: Genode::Attached_io_mem_dataspace(env, _config(index).mmio_base,
		                                     _config(index).mmio_size),
		  Exynos_uart_base((Genode::addr_t)local_addr<void>(),
		                   Genode::Board_base::UART_2_CLOCK,
		                   _baud_rate(baud_rate)),
		  Driver_base(env, _config(index).irq_number, func) {
			_rx_enable(); }


		/***************************
		 ** UART driver interface **
		 ***************************/

		void put_char(char c) override { Exynos_uart_base::put_char(c); }
		bool char_avail()     override { return _rx_avail(); }
		char get_char()       override { return _rx_char();  }
};

#endif /* _UART_DRIVER_H_ */
