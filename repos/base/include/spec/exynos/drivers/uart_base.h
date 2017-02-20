/*
 * \brief  Driver base for the Exynos UART
 * \author Martin stein
 * \date   2013-01-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__EXYNOS__DRIVERS__UART_BASE_H_
#define _INCLUDE__SPEC__EXYNOS__DRIVERS__UART_BASE_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode { class Exynos_uart_base; } 


/**
 * Exynos UART driver base
 */
class Genode::Exynos_uart_base : Mmio
{
	protected:

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
				       Rx_timeout::bits(1);
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
			struct Rx_fifo_count : Bitfield<0, 8>  { };
			struct Rx_fifo_full  : Bitfield<8, 1>  { };
			struct Tx_fifo_full  : Bitfield<24, 1> { };
		};

		/**
		 * Transmit buffer
		 */
		struct Utxh : Register<0x20, 32>
		{
			struct Transmit_data : Bitfield<0, 8> { };
		};

		/**
		 * Receive buffer
		 */
		struct Urxh : Register<0x24, 32>
		{
			struct Receive_data : Bitfield<0, 8> { };
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

		/**
		 * Interrupt mask register
		 */
		template <unsigned OFF>
		struct Uintx : Register<OFF, 32>
		{
			struct Rxd   : Register<OFF, 32>::template Bitfield<0, 1> { };
			struct Error : Register<OFF, 32>::template Bitfield<1, 1> { };
			struct Txd   : Register<OFF, 32>::template Bitfield<2, 1> { };
			struct Modem : Register<OFF, 32>::template Bitfield<3, 1> { };
		};

		using Uintp = Uintx<0x30>;
		using Uintm = Uintx<0x38>;

		void _rx_enable()
		{
			write<Ufcon::Fifo_en>(1);

			/* mask all IRQs except receive IRQ */
			write<Uintm>(Uintm::Error::bits(1) |
			             Uintm::Txd::bits(1) |
			             Uintm::Modem::bits(1));

			/* clear pending IRQs */
			write<Uintp>(Uintp::Rxd::bits(1) |
			             Uintp::Error::bits(1) |
			             Uintp::Txd::bits(1) |
			             Uintp::Modem::bits(1));
		}

		bool _rx_avail() {
			return (read<Ufstat>() & (Ufstat::Rx_fifo_count::bits(0xff)
			        | Ufstat::Rx_fifo_full::bits(1))); }

		/**
		 * Return character received via UART
		 */
		char _rx_char()
		{
			read<Ufcon>();
			char c = read<Urxh::Receive_data>();

			/* clear pending RX IRQ */
			write<Uintp>(Uintp::Rxd::bits(1));
			return c;
		}

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
			/* RX and TX FIFO reset */
			write<Ufcon::Rx_fifo_rst>(1);
			write<Ufcon::Tx_fifo_rst>(1);
			while (read<Ufcon::Rx_fifo_rst>() || read<Ufcon::Tx_fifo_rst>()) ;

			/* init control registers */
			write<Ulcon>(Ulcon::init_value());
			write<Ucon>(Ucon::init_value());
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

#endif /* _INCLUDE__SPEC__EXYNOS__DRIVERS__UART_BASE_H_ */
