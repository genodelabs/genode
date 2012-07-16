/*
 * \brief  Console backend for NOVA
 * \author Norman Feske
 * \date   2009-12-28
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/cap_sel_alloc.h>
#include <base/console.h>

/* NOVA includes */
#include <nova/syscalls.h>

typedef unsigned char uint8_t;


/**
 * Read byte from I/O port
 */
inline uint8_t inb(unsigned short port)
{
	uint8_t res;
	asm volatile ("inb %%dx, %0" :"=a"(res) :"Nd"(port));
	return res;
}


/**
 * Write byte to I/O port
 */
inline void outb(unsigned short port, uint8_t val)
{
	asm volatile ("outb %b0, %w1" : : "a" (val), "Nd" (port));
}


/**
 * Definitions of PC serial ports
 */
enum Comport { COMPORT_0, COMPORT_1, COMPORT_2, COMPORT_3 };

enum {
//	COMPORT_0_BASE = 0x1010,  /* comport of serial PCI card */
	COMPORT_0_BASE = 0x3f8,   /* default comport 0, used wiht Qemu */
	COMPORT_1_BASE = 0x2f8,
	COMPORT_2_BASE = 0x3e8,
	COMPORT_3_BASE = 0x2e8,
	COMPORT_DATA_OFFSET   = 0,
	COMPORT_STATUS_OFFSET = 5,

	STATUS_THR_EMPTY = 0x20,  /* transmitter hold register empty */
	STATUS_DHR_EMPTY = 0x40,  /* data hold register empty - data completely sent */
};


/**
 * Initialize serial port
 *
 * Based on 'init_serial' of L4ka::Pistachio's 'kdb/platform/pc99/io.cc'
 */
static void init_comport(unsigned short port, unsigned baud)
{
	const unsigned
		IER  = port + 1,
		EIR  = port + 2,
		LCR  = port + 3,
		MCR  = port + 4,
		LSR  = port + 5,
		MSR  = port + 6,
		DLLO = port + 0,
		DLHI = port + 1;

	outb(LCR, 0x80);  /* select bank 1 */
	for (volatile int i = 10000000; i--; );
	outb(DLLO, (115200/baud) >> 0);
	outb(DLHI, (115200/baud) >> 8);
	outb(LCR, 0x03);  /* set 8,N,1 */
	outb(IER, 0x00);  /* disable interrupts */
	outb(EIR, 0x07);  /* enable FIFOs */
	outb(MCR, 0x0b);  /* force data terminal ready */
	outb(IER, 0x01);  /* enable RX interrupts */
	inb(IER);
	inb(EIR);
	inb(LCR);
	inb(MCR);
	inb(LSR);
	inb(MSR);
}


/**
 * Output character to serial port
 */
inline void serial_out_char(Comport comport, uint8_t c)
{
	static int io_port[] = { COMPORT_0_BASE, COMPORT_1_BASE,
	                         COMPORT_2_BASE, COMPORT_3_BASE };

	/* wait until serial port is ready */
	uint8_t ready = STATUS_THR_EMPTY;
	while ((inb(io_port[comport] + COMPORT_STATUS_OFFSET) & ready) != ready);

	/* output character */
	outb(io_port[comport] + COMPORT_DATA_OFFSET, c);
}


namespace Genode {

	class Core_console : public Console
	{
		protected:

			void _out_char(char c)
			{
				if (c == '\n')
					serial_out_char(COMPORT_0, '\r');
				serial_out_char(COMPORT_0, c);
			}

		public:

			Core_console() { init_comport(COMPORT_0_BASE, 115200); }
	};
}

