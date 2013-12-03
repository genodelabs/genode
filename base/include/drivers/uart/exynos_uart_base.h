/*
 * \brief  Driver base for the Exynos UART
 * \author Martin stein
 * \date   2013-01-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__UART__EXYNOS_UART_BASE_H_
#define _INCLUDE__DRIVERS__UART__EXYNOS_UART_BASE_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{
	/**
	 * Exynos UART driver base
	 */
	class Exynos_uart_base : Mmio
	{
		/**
		 * Line control
		 */
		struct Ulcon : Register<0x0, 32>
		{
			struct Word_length   : Bitfield<0, 2> { enum { _8_BIT = 3 }; };
			struct Stop_bits     : Bitfield<2, 1> { enum { _1_BIT = 0 }; };
			struct Parity_mode   : Bitfield<3, 3> { enum { NONE = 0 }; };
			struct Infrared_mode : Bitfield<6, 1> { };

			/**
			 * Initialization value
			 */
			static access_t init_value()
			{
				return Word_length::bits(Word_length::_8_BIT) |
				       Stop_bits::bits(Stop_bits::_1_BIT)     |
				       Parity_mode::bits(Parity_mode::NONE)   |
				       Infrared_mode::bits(0);
			}
		};

		/**
		 * Control
		 */
		struct Ucon : Register<0x4, 32>
		{
			struct Receive_mode    : Bitfield<0, 2> { enum { IRQ_POLL = 1 }; };
			struct Transmit_mode   : Bitfield<2, 2> { enum { IRQ_POLL = 1 }; };
			struct Send_brk_signal : Bitfield<4, 1> { };
			struct Loop_back_mode  : Bitfield<5, 1> { };
			struct Rx_err_irq      : Bitfield<6, 1> { };
			struct Rx_timeout      : Bitfield<7, 1> { };
			struct Rx_irq_type     : Bitfield<8, 1> { enum { LEVEL = 1 }; };
			struct Tx_irq_type     : Bitfield<9, 1> { enum { LEVEL = 1 }; };
			struct Rx_to_dma_susp  : Bitfield<10, 1> { };
			struct Rx_to_empty_rx  : Bitfield<11, 1> { };
			struct Rx_to_interval  : Bitfield<12, 4> { };
			struct Rx_dma_bst_size : Bitfield<16, 3> { };
			struct Tx_dma_bst_size : Bitfield<20, 3> { };

			/**
			 * Initialization value
			 */
			static access_t init_value()
			{
				return Receive_mode::bits(Receive_mode::IRQ_POLL)   |
				       Transmit_mode::bits(Transmit_mode::IRQ_POLL) |
				       Send_brk_signal::bits(0)                     |
				       Loop_back_mode::bits(0)                      |
				       Rx_err_irq::bits(1)                          |
				       Rx_timeout::bits(0)                          |
				       Rx_irq_type::bits(Rx_irq_type::LEVEL)        |
				       Tx_irq_type::bits(Tx_irq_type::LEVEL)        |
				       Rx_to_dma_susp::bits(0)                      |
				       Rx_to_empty_rx::bits(0)                      |
				       Rx_to_interval::bits(3)                      |
				       Rx_dma_bst_size::bits(0)                     |
				       Tx_dma_bst_size::bits(0);
			}
		};

		/**
		 * FIFO control
		 */
		struct Ufcon : Register<0x8, 32>
		{
			struct Fifo_en         : Bitfield<0, 1> { };
			struct Rx_fifo_rst     : Bitfield<1, 1> { };
			struct Tx_fifo_rst     : Bitfield<2, 1> { };
			struct Rx_fifo_trigger : Bitfield<4, 3> { };
			struct Tx_fifo_trigger : Bitfield<8, 3> { };

			/**
			 * Initialization value
			 */
			static access_t init_value()
			{
				return Fifo_en::bits(1)         |
				       Rx_fifo_rst::bits(0)     |
				       Tx_fifo_rst::bits(0)     |
				       Rx_fifo_trigger::bits(0) |
				       Tx_fifo_trigger::bits(0);
			}
		};

		/**
		 * Modem control
		 */
		struct Umcon : Register<0xc, 32>
		{
			struct Send_request  : Bitfield<0, 1> { };
			struct Modem_irq     : Bitfield<3, 1> { };
			struct Auto_flow_ctl : Bitfield<4, 1> { };
			struct Rts_trigger   : Bitfield<5, 3> { };

			/**
			 * Initialization value
			 */
			static access_t init_value()
			{
				return Send_request::bits(0)  |
				       Modem_irq::bits(0)     |
				       Auto_flow_ctl::bits(0) |
				       Rts_trigger::bits(0);
			}
		};

		/**
		 * FIFO status
		 */
		struct Ufstat : Register<0x18, 32>
		{
			struct Tx_fifo_full : Bitfield<24, 1> { };
		};

		/**
		 * Transmit buffer
		 */
		struct Utxh : Register<0x20, 32>
		{
			struct Transmit_data : Bitfield<0, 8> { };
		};

		/**
		 * Baud Rate Divisor
		 */
		struct Ubrdiv : Register<0x28, 32>
		{
			struct Baud_rate_div : Bitfield<0, 16> { };
		};

		/**
		 * Fractional part of Baud Rate Divisor
		 */
		struct Ufracval : Register<0x2c, 32>
		{
			struct Baud_rate_frac : Bitfield<0, 4> { };
		};

		public:

			/**
			 * Constructor
			 *
			 * \param  base       MMIO base address
			 * \param  clock      reference clock
			 * \param  baud_rate  targeted baud rate
			 */
			Exynos_uart_base(addr_t const base, unsigned const clock,
			                  unsigned const baud_rate) : Mmio(base)
			{
				/* init control registers */
				write<Ulcon>(Ulcon::init_value());
				write<Ucon>(Ucon::init_value());
				write<Ufcon>(Ufcon::init_value());
				write<Umcon>(Umcon::init_value());

				/* apply baud rate */
				float const div_val = ((float)clock / (baud_rate * 16)) - 1;
				Ubrdiv::access_t const ubrdiv = div_val;
				Ufracval::access_t const ufracval =
					((float)div_val - ubrdiv) * 16;
				write<Ubrdiv::Baud_rate_div>(ubrdiv);
				write<Ufracval::Baud_rate_frac>(ufracval);
			}

			/**
			 * Print character 'c' through the UART
			 */
			void put_char(char const c)
			{
				while (read<Ufstat::Tx_fifo_full>()) ;
				write<Utxh::Transmit_data>(c);
			}
	};
}

#endif /* _INCLUDE__DRIVERS__UART__EXYNOS_UART_BASE_H_ */

