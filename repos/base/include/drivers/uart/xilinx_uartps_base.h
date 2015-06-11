/*
 * \brief  Base UART driver for the Xilinx UART PS used on Zynq devices
 * \author Johannes Schlatow
 * \date   2014-12-15
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__UART__XILINX_UARTPS_BASE_H_
#define _INCLUDE__DRIVERS__UART__XILINX_UARTPS_BASE_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{
	/**
	 * Base driver Xilinx UART PS module
	 */
	class Xilinx_uartps_base : public Mmio
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
				struct Rx_disable: Bitfield<3, 1> { };
				struct Tx_enable : Bitfield<4, 1> { };
				struct Tx_disable: Bitfield<5, 1> { };
				struct Res_to    : Bitfield<6, 1> { };
				struct Start_tbrk: Bitfield<7, 1> { };
				struct Stop_tbrk : Bitfield<8, 1> { };
			};

			/**
			* Mode register
			*/
			struct Uart_mr : Register<0x04, 32>
			{
				struct Clock_sel  : Bitfield<0, 1> { };
				struct Char_len   : Bitfield<1, 2> {
					enum {
						LEN_8_BIT = 0,
						LEN_7_BIT = 2,
						LEN_6_BIT = 3
					};
				};
				struct Parity     : Bitfield<3, 3> {
					enum {
						EVEN      = 0,
						ODD       = 1,
						FORCED_0  = 2,
						FORCED_1  = 3,
						NO_PARITY = 4
					};
				};
				struct Nbr_stop   : Bitfield<6, 2> {
					enum {
						ONE          = 0,
						ONE_AND_HALF = 1,
						TWO          = 2
					};
				};
				struct Chan_mode  : Bitfield<8, 2> {
					enum {
						NORMAL          = 0,
						AUTO_ECHO       = 1,
						LOCAL_LOOPBACK  = 2,
						REMOTE_LOOPBACK = 3
					};
				};
			};

			/**
			* Baudgen register
			*/
			struct Uart_baudgen : Register<0x18, 32>
			{
				struct Clock_div  : Bitfield<0, 16> { };
			};

			/**
			* Status register
			*/
			struct Uart_sr       : Register<0x2C, 32>
			{
				struct Rx_trig    : Bitfield<0, 1> { };
				struct Rx_empty   : Bitfield<1, 1> { };
				struct Rx_full    : Bitfield<2, 1> { };
				struct Tx_empty   : Bitfield<3, 1> { };
				struct Tx_full    : Bitfield<4, 1> { };
				struct Rx_active  : Bitfield<10,1> { };
				struct Tx_active  : Bitfield<11,1> { };
				struct Flow_delay : Bitfield<12,1> { };
				struct Ttrig      : Bitfield<13,1> { };
				struct Tnful      : Bitfield<14,1> { };
			};

			/**
			* FIFO register
			*/
			struct Uart_fifo : Register<0x30, 32>
			{
				struct Data   : Bitfield<0, 8> { };
			};

			/**
			* Bauddiv register
			*/
			struct Uart_bauddiv : Register<0x34, 32>
			{
				struct Bdiv : Bitfield<0,8> { };
			};

			void _init(unsigned long const clock, unsigned long const baud_rate)
			{
				/* reset UART */
				write<Uart_cr>(Uart_cr::Tx_reset::bits(1)
				             | Uart_cr::Rx_reset::bits(1));

				/* set baud rate */
				unsigned div = 4;
				write<Uart_bauddiv::Bdiv>(div);
				write<Uart_baudgen::Clock_div>(clock / baud_rate / (div + 1));

				/* set 8N1 */
				write<Uart_mr>(Uart_mr::Parity::bits(Uart_mr::Parity::NO_PARITY)
				             | Uart_mr::Char_len::bits(Uart_mr::Char_len::LEN_8_BIT)
								 | Uart_mr::Nbr_stop::bits(Uart_mr::Nbr_stop::ONE));

				/* enable */
				write<Uart_cr>(Uart_cr::Rx_enable::bits(1)
				             | Uart_cr::Tx_enable::bits(1));

			}

		public:
			/**
			 * Constructor
			 *
			 * \param  base       MMIO base address
			 * \param  clock      reference clock
			 * \param  baud_rate  targeted baud rate
			 */
			Xilinx_uartps_base(addr_t const base, unsigned long const clock,
			              unsigned long const baud_rate) : Mmio(base)
			{
				/* reset and init UART */
				_init(clock, baud_rate);
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
}

#endif /* _INCLUDE__DRIVERS__UART__XILINX_UARTPS_BASE_H_ */

