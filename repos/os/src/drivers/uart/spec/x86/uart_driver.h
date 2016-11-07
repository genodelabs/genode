/*
 * \brief  i8250 UART driver
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2011-09-12
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UART_DRIVER_H_
#define _UART_DRIVER_H_

/* Genode includes */
#include <base/env.h>
#include <io_port_session/connection.h>

enum { UARTS_NUM = 4 }; /* needed by base class definitions */

/* local includes */
#include <uart_driver_base.h>

class Uart::Driver : public Uart::Driver_base
{
	private:

		unsigned                   _port_base;
		Genode::Io_port_connection _io_port;

		/**
		 * Return I/O port base for specified UART
		 */
		static unsigned _io_port_base(int index)
		{
			static unsigned port_base[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
			return port_base[index & 0x3];
		}

		/**
		 * Return irq number for specified UART
		 */
		static int _irq_number(int index)
		{
			static int irq[] = { 4, 3, 4, 3 };
			Genode::log("open IRQ ", irq[index & 0x3], "\n");
			return irq[index & 0x3];
		}

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

		enum { BAUD_115200 = 115200 };

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

		unsigned _baud_rate(unsigned baud_rate)
		{
			if (baud_rate != BAUD_115200)
				Genode::warning("baud_rate ", baud_rate,
				                " not supported, set to default\n");
			return BAUD_115200;
		}

	public:

		Driver(Genode::Env &env, unsigned index, unsigned baud,
		       Uart::Char_avail_functor &func)
		: Driver_base(env, _irq_number(index), func),
		  _port_base(_io_port_base(index)),
		  _io_port(_port_base, 0xf) {
			_init_comport(_baud_rate(baud)); }


		/***************************
		 ** UART driver interface **
		 ***************************/

		void put_char(char c) override
		{
			/* wait until serial port is ready */
			while (!(_inb<LSR>() & 0x60));

			/* output character */
			_outb<TRB>(c);
		}

		bool char_avail() override
		{
			return _inb<LSR>() & 1;
		}

		char get_char() override
		{
			return _inb<TRB>();
		}

		void baud_rate(int bits_per_second) override
		{
			_init_comport(bits_per_second);
		}
};

#endif /* _UART_DRIVER_H_ */
