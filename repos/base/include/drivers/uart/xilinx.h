/*
 * \brief  Base UART driver for the Xilinx UART PS used on Zynq devices
 * \author Johannes Schlatow
 * \date   2014-12-15
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__UART__XILINX_H_
#define _INCLUDE__DRIVERS__UART__XILINX_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode { class Xilinx_uart; }

/**
 * Base driver Xilinx UART PS module
 */
class Genode::Xilinx_uart: public Mmio
{
	protected:

		/**
		 * Control register
		 */
		struct Uart_cr : Register<0x00, 32>
		{
			struct Rx_reset  : Bitfield<0, 1> { };
			struct Tx_reset  : Bitfield<1, 1> { };
			struct Rx_enable : Bitfield<2, 1> { };
			struct Tx_enable : Bitfield<4, 1> { };
		};

		/**
		 * Mode register
		 */
		struct Uart_mr : Register<0x04, 32>
		{
			struct Clock_sel : Bitfield<0, 1> { };
			struct Parity    : Bitfield<3, 3> { enum { NO_PARITY = 4 }; };
		};

		/**
		 * Baudgen register
		 */
		struct Uart_baudgen : Register<0x18, 32>
		{
			struct Clock_div : Bitfield<0, 16> { };
		};

		/**
		 * Status register
		 */
		struct Uart_sr : Register<0x2C, 32>
		{
			struct Tx_full : Bitfield<4, 1> { };
		};

		/**
		 * FIFO register
		 */
		struct Uart_fifo : Register<0x30, 32>
		{
			struct Data : Bitfield<0, 8> { };
		};

		/**
		 * Bauddiv register
		 */
		struct Uart_bauddiv : Register<0x34, 32>
		{
			struct Bdiv : Bitfield<0,8> { };
		};

	public:

		/**
		 * Constructor
		 *
		 * \param  base       MMIO base address
		 * \param  clock      reference clock
		 * \param  baud_rate  targeted baud rate
		 */
		Xilinx_uart(addr_t const base, unsigned long const clock,
		            unsigned long const baud_rate) : Mmio(base)
		{
			/* reset UART */
			Uart_cr::access_t uart_cr = 0;
			Uart_cr::Tx_reset::set(uart_cr, 1);
			Uart_cr::Rx_reset::set(uart_cr, 1);
			write<Uart_cr>(uart_cr);

			/* set baud rate */
			constexpr unsigned div = 4;
			write<Uart_bauddiv::Bdiv>(div);
			write<Uart_baudgen::Clock_div>(clock / baud_rate / (div + 1));

			/* set 8N1 */
			Uart_mr::access_t uart_mr = 0;
			Uart_mr::Parity::set(uart_mr, Uart_mr::Parity::NO_PARITY);
			write<Uart_mr>(uart_mr);

			/* enable */
			uart_cr = 0;
			Uart_cr::Rx_enable::set(uart_cr, 1);
			Uart_cr::Tx_enable::set(uart_cr, 1);
			write<Uart_cr>(uart_cr);
		}

		/**
		 * Transmit ASCII char 'c'
		 */
		void put_char(char const c)
		{
			/* wait as long as the transmission buffer is full */
			while (read<Uart_sr::Tx_full>()) ;

			/* transmit character */
			write<Uart_fifo::Data>(c);
		}
};

#endif /* _INCLUDE__DRIVERS__UART__XILINX_H_ */
