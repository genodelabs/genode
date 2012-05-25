/*
 * \brief  Base UART driver for the Texas instruments TL16C750 module
 * \author Martin stein
 * \date   2011-10-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__UART__TL16C750_BASE_H_
#define _INCLUDE__DRIVERS__UART__TL16C750_BASE_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{
	enum {
		ASCII_LINE_FEED = 10,
		ASCII_CARRIAGE_RETURN = 13,
	};

	/**
	 * Base driver Texas instruments TL16C750 UART module
	 *
	 * In contrast to the abilities of the TL16C750, this driver targets only
	 * the basic UART functionalities.
	 */
	class Tl16c750_base : public Mmio
	{
		/**
		 * Least significant divisor part
		 */
		struct Uart_dll : Register<0x0, 32>
		{
			struct Clock_lsb : Bitfield<0, 8> { };
		};

		/**
		 * Transmit holding register
		 */
		struct Uart_thr : Register<0x0, 32>
		{
			struct Thr : Bitfield<0, 8> { };
		};

		/**
		 * Most significant divisor part
		 */
		struct Uart_dlh : Register<0x4, 32>
		{
			struct Clock_msb : Bitfield<0, 6> { };
		};

		/**
		 * Interrupt enable register
		 */
		struct Uart_ier : Register<0x4, 32>
		{
			struct Rhr_it : Bitfield<0, 1> { };
			struct Thr_it : Bitfield<1, 1> { };
			struct Line_sts_it : Bitfield<2, 1> { };
			struct Modem_sts_it : Bitfield<3, 1> { };
			struct Sleep_mode : Bitfield<4, 1> { };
			struct Xoff_it : Bitfield<5, 1> { };
			struct Rts_it : Bitfield<6, 1> { };
			struct Cts_it : Bitfield<7, 1> { };
		};

		/**
		 * FIFO control register
		 */
		struct Uart_fcr : Register<0x8, 32>
		{
			struct Fifo_enable   : Bitfield<0, 1> { };
		};

		/**
		 * Line control register
		 */
		struct Uart_lcr : Register<0xc, 32>
		{
			struct Char_length : Bitfield<0, 2>
			{
				enum { _8_BIT = 3 };
			};
			struct Nb_stop : Bitfield<2, 1>
			{
				enum { _1_STOP_BIT = 0 };
			};
			struct Parity_en : Bitfield<3, 1> { };
			struct Break_en  : Bitfield<6, 1> { };
			struct Div_en    : Bitfield<7, 1> { };
			struct Reg_mode  : Bitfield<0, 8>
			{
				enum { OPERATIONAL = 0, CONFIG_A = 0x80, CONFIG_B = 0xbf };
			};
		};

		/**
		 * Modem control register
		 */
		struct Uart_mcr : Register<0x10, 32>
		{
			struct Tcr_tlr : Bitfield<6, 1> { };
		};

		/**
		 * Line status register
		 */
		struct Uart_lsr : Register<0x14, 32>
		{
			struct Tx_fifo_empty : Bitfield<5, 1> { };
		};

		/**
		 * Mode definition register 1
		 */
		struct Uart_mdr1 : Register<0x20, 32>
		{
			struct Mode_select : Bitfield<0, 3>
			{
				enum { UART_16X = 0, DISABLED = 7 };
			};
		};

		/**
		 * System control register
		 */
		struct Uart_sysc : Register<0x54, 32>
		{
			struct Softreset : Bitfield<1, 1> { };
		};

		/**
		 * System status register
		 */
		struct Uart_syss : Register<0x58, 32>
		{
			struct Resetdone : Bitfield<0, 1> { };
		};

		public:

			/**
			 * Constructor
			 *
			 * \param  base       MMIO base address
			 * \param  clock      reference clock
			 * \param  baud_rate  targeted baud rate
			 */
			Tl16c750_base(addr_t const base, unsigned long const clock,
			              unsigned long const baud_rate) : Mmio(base)
			{
				/* reset and disable UART */
				write<Uart_sysc::Softreset>(1);
				while (!read<Uart_syss::Resetdone>()) ;
				write<Uart_mdr1::Mode_select>(Uart_mdr1::Mode_select::DISABLED);

				/* enable access to 'Uart_fcr' and 'Uart_ier' */
				write<Uart_lcr::Reg_mode>(Uart_lcr::Reg_mode::OPERATIONAL);

				/*
				 * Configure FIFOs, we don't use any interrupts or DMA,
				 * thus FIFO trigger and DMA configurations are dispensable.
				 */
				write<Uart_fcr::Fifo_enable>(1);

				/* disable interrupts and sleep mode */
				write<Uart_ier>(Uart_ier::Rhr_it::bits(0)
				              | Uart_ier::Thr_it::bits(0)
				              | Uart_ier::Line_sts_it::bits(0)
				              | Uart_ier::Modem_sts_it::bits(0)
				              | Uart_ier::Sleep_mode::bits(0)
				              | Uart_ier::Xoff_it::bits(0)
				              | Uart_ier::Rts_it::bits(0)
				              | Uart_ier::Cts_it::bits(0));

				/* enable access to 'Uart_dlh' and 'Uart_dll' */
				write<Uart_lcr::Reg_mode>(Uart_lcr::Reg_mode::CONFIG_B);

				/*
				 * Load the new divisor value (this driver solely uses
				 * 'UART_16X' mode)
				 */
				enum { UART_16X_DIVIDER_LOG2 = 4 };
				unsigned long const adjusted_br = baud_rate << UART_16X_DIVIDER_LOG2;
				double const divisor = (double)clock / adjusted_br;
				unsigned long const divisor_uint = (unsigned long)divisor;
				write<Uart_dll::Clock_lsb>(divisor_uint);
				write<Uart_dlh::Clock_msb>(divisor_uint>>Uart_dll::Clock_lsb::WIDTH);

				/*
				 * Configure protocol formatting and thereby return to
				 * operational mode.
				 */
				write<Uart_lcr>(Uart_lcr::Char_length::bits(Uart_lcr::Char_length::_8_BIT)
				              | Uart_lcr::Nb_stop::bits(Uart_lcr::Nb_stop::_1_STOP_BIT)
				              | Uart_lcr::Parity_en::bits(0)
				              | Uart_lcr::Break_en::bits(0)
				              | Uart_lcr::Div_en::bits(0));

				/*
				 * Switch to UART mode, we don't use hardware or software flow
				 * control, thus according configurations are dispensable
				 */
				write<Uart_mdr1::Mode_select>(Uart_mdr1::Mode_select::UART_16X);
			}

			/**
			 * Transmit ASCII char 'c'
			 */
			void put_char(char const c)
			{
				/* wait as long as the transmission buffer is full */
				while (!read<Uart_lsr::Tx_fifo_empty>()) ;

				/* auto complete new line commands */
				if (c == ASCII_LINE_FEED)
					write<Uart_thr::Thr>(ASCII_CARRIAGE_RETURN);

				/* transmit character */
				write<Uart_thr::Thr>(c);
			}
	};
}

#endif /* _INCLUDE__DRIVERS__UART__TL16C750_BASE_H_ */

