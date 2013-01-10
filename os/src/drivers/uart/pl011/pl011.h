/*
 * \brief  PL011 UART driver
 * \author Christian Helmuth
 * \date   2011-05-27
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PL011_H_
#define _PL011_H_

/* Genode includes */
#include <base/env.h>
#include <base/lock.h>
#include <base/printf.h>
#include <os/irq_activation.h>
#include <os/attached_io_mem_dataspace.h>

/* local includes */
#include "uart_driver.h"

class Pl011 : public Uart::Driver, public Genode::Irq_handler
{
	private:

		/*
		 * Constants for flags, registers, etc. only visible in this file are
		 * named as in the specification documents to relieve correlation
		 * between implementation and specification.
		 */

		/**
		 * Register offsets
		 *
		 * Registers are read/writable unless explicitly stated.
		 */
		enum Register {
			UARTDR    = 0x000,  /* data */
			UARTFR    = 0x018,  /* flags (read-only) */
			UARTIBRD  = 0x024,  /* integer baud rate divisor */
			UARTFBRD  = 0x028,  /* fractional baud rate divisor */
			UARTLCR_H = 0x02c,  /* line control */
			UARTCR    = 0x030,  /* control */
			UARTIMSC  = 0x038,  /* interrupt mask register */
			UARTICR   = 0x044,  /* interrupt clear register */
		};

		/**
		 * Flags
		 */
		enum Flag {
			/* flag register */
			UARTFR_BUSY      = 0x0008,  /* busy on tx */
			UARTFR_TXFF      = 0x0020,  /* tx FIFO full */
			UARTFR_RXFE      = 0x0010,  /* rx FIFO empty */

			/* line control register */
			UARTLCR_H_FEN    = 0x0010,  /* enable FIFOs */
			UARTLCR_H_WLEN_8 = 0x0060,  /* 8 bit word length */

			/* control register */
			UARTCR_UARTEN    = 0x0001,  /* enable uart */
			UARTCR_TXE       = 0x0100,  /* enable tx */
			UARTCR_RXE       = 0x0200,  /* enable rx */

			/* interrupt mask register */
			UARTIMSC_RXIM    = 0x10,    /* rx interrupt mask */

			/* interrupt clear register */
			UARTICR_RXIC     = 0x10,    /* rx interrupt clear */
		};

		Genode::Attached_io_mem_dataspace  _io_mem;
		Genode::uint32_t volatile         *_base;

		Uart::Char_avail_callback &_char_avail_callback;

		enum { IRQ_STACK_SIZE = 4096 };
		Genode::Irq_activation _irq_activation;

		Genode::uint32_t _read_reg(Register reg) const
		{
			return _base[reg >> 2];
		}

		void _write_reg(Register reg, Genode::uint32_t v)
		{
			_base[reg >> 2] = v;
		}

	public:

		/**
		 * Constructor
		 */
		Pl011(Genode::addr_t mmio_base, Genode::size_t mmio_size,
		      unsigned ibrd, unsigned fbrd, int irq_number,
		      Uart::Char_avail_callback &callback)
		:
			_io_mem(mmio_base, mmio_size),
			_base(_io_mem.local_addr<unsigned volatile>()),
			_char_avail_callback(callback),
			_irq_activation(irq_number, *this, IRQ_STACK_SIZE)
		{
			/* disable and flush uart */
			_write_reg(UARTCR, 0);
			while (_read_reg(UARTFR) & UARTFR_BUSY) ;
			_write_reg(UARTLCR_H, 0);

			/* set baud-rate divisor */
			_write_reg(UARTIBRD, ibrd);
			_write_reg(UARTFBRD, fbrd);

			/* enable FIFOs, 8-bit words */
			_write_reg(UARTLCR_H, UARTLCR_H_FEN | UARTLCR_H_WLEN_8);

			/* enable transmission */
			_write_reg(UARTCR, UARTCR_TXE);

			/* enable uart */
			_write_reg(UARTCR, _read_reg(UARTCR) | UARTCR_UARTEN);

			/* enable rx interrupt */
			_write_reg(UARTIMSC, UARTIMSC_RXIM);
		}

		/***************************
		 ** IRQ handler interface **
		 ***************************/

		void handle_irq(int irq_number)
		{
			/* inform client about the availability of data */
			_char_avail_callback();

			/* acknowledge irq */
			_write_reg(UARTICR, UARTICR_RXIC);
		}

		/***************************
		 ** UART driver interface **
		 ***************************/

		void put_char(char c)
		{
			/* spin while FIFO full */
			while (_read_reg(UARTFR) & UARTFR_TXFF) ;

			_write_reg(UARTDR, c);
		}

		bool char_avail()
		{
			return !(_read_reg(UARTFR) & UARTFR_RXFE);
		}

		char get_char()
		{
			return _read_reg(UARTDR);
		}
};

#endif /* _PL011_H_ */
