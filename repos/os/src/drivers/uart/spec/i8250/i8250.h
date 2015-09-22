/*
 * \brief  i8250 UART driver
 * \author Norman Feske
 * \date   2011-09-12
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__UART__SPEC__I8250__I8250_H_
#define _DRIVERS__UART__SPEC__I8250__I8250_H_

/* Genode includes */
#include <base/env.h>
#include <base/lock.h>
#include <base/printf.h>
#include <os/irq_activation.h>
#include <io_port_session/connection.h>

/* local includes */
#include <uart_driver.h>


class I8250 : public Uart::Driver, public Genode::Irq_handler
{
	private:

		unsigned _port_base;

		Genode::Io_port_connection _io_port;

		/**
		 * Register offsets relative to '_port_base'
		 */
		enum Reg {
			TRB  = 0,  /* transmit/receive buffer */
			IER  = 1,
			EIR  = 2,
			LCR  = 3,
			MCR  = 4,
			LSR  = 5,  /* line status register */
			MSR  = 6,
			DLLO = 0,
			DLHI = 1,
		};

		/**
		 * Read byte from i8250 register
		 */
		template <Reg REG>
		char _inb() { return _io_port.inb(_port_base + REG); }

		/**
		 * Write byte to i8250 register
		 */
		template <Reg REG>
		void _outb(char value) { _io_port.outb(_port_base + REG, value); }

		/**
		 * Initialize UART
		 *
		 * Based on 'init_serial' of L4ka::Pistachio's 'kdb/platform/pc99/io.cc'
		 */
		void _init_comport(unsigned baud)
		{
			_outb<LCR>(0x80);  /* select bank 1 */
			for (volatile int i = 10000000; i--; );
			_outb<DLLO>((115200/baud) >> 0);
			_outb<DLHI>((115200/baud) >> 8);
			_outb<LCR>(0x03);  /* set 8,N,1 */
			_outb<IER>(0x00);  /* disable interrupts */
			_outb<EIR>(0x07);  /* enable FIFOs */
			_outb<MCR>(0x0b);  /* force data terminal ready */
			_outb<IER>(0x01);  /* enable RX interrupts */
			_inb<IER>();
			_inb<EIR>();
			_inb<LCR>();
			_inb<MCR>();
			_inb<LSR>();
			_inb<MSR>();
		}

		Uart::Char_avail_callback &_char_avail_callback;

		enum { IRQ_STACK_SIZE = 4096 };
		Genode::Irq_activation _irq_activation;

	public:

		/**
		 * Constructor
		 */
		I8250(unsigned port_base, int irq_number, unsigned baud,
		      Uart::Char_avail_callback &callback)
		:
			_port_base(port_base),
			_io_port(port_base, 0xf),
			_char_avail_callback(callback),
			_irq_activation(irq_number, *this, IRQ_STACK_SIZE)
		{
			_init_comport(baud);
		}

		/***************************
		 ** IRQ handler interface **
		 ***************************/

		void handle_irq(int irq_number)
		{
			/* inform client about the availability of data */
			_char_avail_callback();
		}

		/***************************
		 ** UART driver interface **
		 ***************************/

		void put_char(char c)
		{
			/* wait until serial port is ready */
			while (!(_inb<LSR>() & 0x60));

			/* output character */
			_outb<TRB>(c);
		}

		bool char_avail()
		{
			return _inb<LSR>() & 1;
		}

		char get_char()
		{
			return _inb<TRB>();
		}

		void baud_rate(int bits_per_second)
		{
			_init_comport(bits_per_second);
		}
};

#endif /* _DRIVERS__UART__SPEC__I8250__I8250_H_ */
