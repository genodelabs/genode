/*
 * \brief  Driver for OMAP4 UARTs
 * \author Ivan Loskutov
 * \author Stefan Kalkowski
 * \date   2012-11-08
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UART_DRIVER_H_
#define _UART_DRIVER_H_

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/env.h>
#include <drivers/uart_base.h>
#include <spec/panda/drivers/board_base.h>

enum { UARTS_NUM = 4 }; /* needed by base class definitions */

/* local includes */
#include <uart_driver_base.h>

class Uart::Driver : public Genode::Attached_io_mem_dataspace,
                     public Genode::Tl16c750_base,
                     public Uart::Driver_base
{
	private:

		void _enable_rx_interrupt()
		{
			/* enable access to 'Uart_fcr' and 'Uart_ier' */
			write<Uart_lcr::Reg_mode>(Uart_lcr::Reg_mode::OPERATIONAL);

			/* enable rx interrupt, disable other interrupts and sleep mode */
			write<Uart_ier>(Uart_ier::Rhr_it::bits(1)
			              | Uart_ier::Thr_it::bits(0)
			              | Uart_ier::Line_sts_it::bits(0)
			              | Uart_ier::Modem_sts_it::bits(0)
			              | Uart_ier::Sleep_mode::bits(0)
			              | Uart_ier::Xoff_it::bits(0)
			              | Uart_ier::Rts_it::bits(0)
			              | Uart_ier::Cts_it::bits(0));
			/*
			 * Configure protocol formatting and thereby return to
			 * operational mode.
			 */
			write<Uart_lcr>(Uart_lcr::Char_length::bits(Uart_lcr::Char_length::_8_BIT)
			              | Uart_lcr::Nb_stop::bits(Uart_lcr::Nb_stop::_1_STOP_BIT)
			              | Uart_lcr::Parity_en::bits(0)
			              | Uart_lcr::Break_en::bits(0)
			              | Uart_lcr::Div_en::bits(0));
		}

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
				{ Board_base::TL16C750_1_MMIO_BASE, Board_base::TL16C750_MMIO_SIZE,
				  Board_base::TL16C750_1_IRQ },
				{ Board_base::TL16C750_2_MMIO_BASE, Board_base::TL16C750_MMIO_SIZE,
				  Board_base::TL16C750_2_IRQ },
				{ Board_base::TL16C750_3_MMIO_BASE, Board_base::TL16C750_MMIO_SIZE,
				  Board_base::TL16C750_3_IRQ },
				{ Board_base::TL16C750_4_MMIO_BASE, Board_base::TL16C750_MMIO_SIZE,
				  Board_base::TL16C750_4_IRQ },
			};
			return cfg[index];
		}

		unsigned _baud_rate(unsigned baud_rate)
		{
			if (baud_rate != 0) return baud_rate;

			Genode::warning("baud_rate ", baud_rate,
			                " not supported, set to default");
			return BAUD_115200;
		}

	public:

		Driver(Genode::Env &env, unsigned index,
		       unsigned baud_rate, Char_avail_functor &func)
		: Genode::Attached_io_mem_dataspace(env, _config(index).mmio_base,
		                                     _config(index).mmio_size),
		  Tl16c750_base((Genode::addr_t)local_addr<void>(),
		                Genode::Board_base::TL16C750_CLOCK,
		                _baud_rate(baud_rate)),
		  Driver_base(env, _config(index).irq_number, func) {
			_enable_rx_interrupt(); }


		/***************************
		 ** UART driver interface **
		 ***************************/

		void handle_irq() override
		{
			/* check for interrupt */
			unsigned int iir = read<Uart_iir::It_pending>();
			if (iir) return;

			/* inform client about the availability of data */
			Driver_base::handle_irq();
		}

		void put_char(char c) override { Tl16c750_base::put_char(c); }

		bool char_avail() override { return read<Uart_lsr::Rx_fifo_empty>(); }

		char get_char() override { return read<Uart_rhr::Rhr>(); }

		void baud_rate(int bits_per_second) override
		{
			_init(Genode::Board_base::TL16C750_CLOCK, bits_per_second);
			_enable_rx_interrupt();
		}
};

#endif /* _UART_DRIVER_H_ */
