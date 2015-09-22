/*
 * \brief  Driver for OMAP4 UARTs
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2012-11-08
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__UART__SPEC__OMAP4__OMAP_UART_H_
#define _DRIVERS__UART__SPEC__OMAP4__OMAP_UART_H_

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <os/irq_activation.h>
#include <os/attached_io_mem_dataspace.h>

#include <drivers/uart_base.h>

/* local includes */
#include "uart_driver.h"

class Omap_uart : public Genode::Tl16c750_base, public Uart::Driver, public Genode::Irq_handler
{
	private:
		Genode::Attached_io_mem_dataspace &_uart_mmio;

		Uart::Char_avail_callback &_char_avail_callback;

		enum { IRQ_STACK_SIZE = 4096 };
		Genode::Irq_activation _irq_activation;

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

	public:

		/**
		 * Constructor
		 */
		Omap_uart(Genode::Attached_io_mem_dataspace *uart_mmio, int irq_number,
		          unsigned baud_rate, Uart::Char_avail_callback &callback)
		:
			Tl16c750_base((Genode::addr_t)uart_mmio->local_addr<void>(), Genode::Board_base::TL16C750_CLOCK, baud_rate),
			_uart_mmio(*uart_mmio),
			_char_avail_callback(callback),
			_irq_activation(irq_number, *this, IRQ_STACK_SIZE)
		{
			_enable_rx_interrupt();
		}

		/**
		 * * IRQ handler interface **
		 */
		void handle_irq(int irq_number)
		{
			/* inform client about the availability of data */
			unsigned int iir = read<Uart_iir::It_pending>();
			if (iir) return;
			_char_avail_callback();
		}

		/**
		 * * UART driver interface **
		 */
		void put_char(char c) { Tl16c750_base::put_char(c); }

		bool char_avail() { return read<Uart_lsr::Rx_fifo_empty>(); }

		char get_char() { return read<Uart_rhr::Rhr>(); }

		void baud_rate(int bits_per_second)
		{
			_init(Genode::Board_base::TL16C750_CLOCK, bits_per_second);
			_enable_rx_interrupt();
		}
};

#endif /* _DRIVERS__UART__SPEC__OMAP4__OMAP_UART_H_ */
