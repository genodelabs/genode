/*
 * \brief  UART driver for the x86 PC
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2009-12-28
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__UART__X86_PC_H_
#define _INCLUDE__DRIVERS__UART__X86_PC_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode { class X86_uart; }

class Genode::X86_uart
{
	private:

		uint16_t _port;

		enum {
			COMPORT_DATA_OFFSET   = 0,
			COMPORT_STATUS_OFFSET = 5,

			STATUS_THR_EMPTY = 0x20,  /* transmitter hold register empty */
			STATUS_DHR_EMPTY = 0x40,  /* data hold register empty - data completely sent */
		};


		/**
		 * Read byte from I/O port
		 */
		uint8_t _inb(uint16_t port)
		{
			uint8_t res;
			asm volatile ("inb %%dx, %0" :"=a"(res) :"Nd"(port));
			return res;
		}


		/**
		 * Write byte to I/O port
		 */
		void _outb(uint16_t port, uint8_t val)
		{
			asm volatile ("outb %b0, %w1" : : "a" (val), "Nd" (port));
		}

	public:

		X86_uart(addr_t const port, unsigned const clock,
		         unsigned const baud_rate)
		: _port(port)
		{

			/**
			 * Initialize serial port
			 *
			 * Based on 'init_serial' of L4ka::Pistachio's 'kdb/platform/pc99/io.cc'
			 */

			if (!port)
				return;

			const unsigned
				IER  = port + 1,
				EIR  = port + 2,
				LCR  = port + 3,
				MCR  = port + 4,
				LSR  = port + 5,
				MSR  = port + 6,
				DLLO = port + 0,
				DLHI = port + 1;

			_outb(LCR, 0x80);  /* select bank 1 */
			for (volatile int i = 10000000; i--; );
			_outb(DLLO, (115200/baud_rate) >> 0);
			_outb(DLHI, (115200/baud_rate) >> 8);
			_outb(LCR, 0x03);  /* set 8,N,1 */
			_outb(IER, 0x00);  /* disable interrupts */
			_outb(EIR, 0x07);  /* enable FIFOs */
			_outb(MCR, 0x0b);  /* force data terminal ready */
			_outb(IER, 0x01);  /* enable RX interrupts */
			_inb(IER);
			_inb(EIR);
			_inb(LCR);
			_inb(MCR);
			_inb(LSR);
			_inb(MSR);
		}

		void put_char(char const c)
		{
			if (!_port)
				return;

			/* wait until serial port is ready */
			Genode::uint8_t ready = STATUS_THR_EMPTY;
			while ((_inb(_port + COMPORT_STATUS_OFFSET) & ready) != ready);

			/* output character */
			_outb(_port + COMPORT_DATA_OFFSET, c);
		}
};

#endif /* _INCLUDE__DRIVERS__UART__X86_PC_H_ */
