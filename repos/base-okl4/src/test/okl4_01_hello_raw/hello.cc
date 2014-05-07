/*
 * \brief  Simple roottask replacement for OKL4 that just prints some text
 * \author Norman Feske
 * \date   2008-09-01
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

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


/**
 * Output character to serial port
 */
inline void serial_out_char(Comport comport, uint8_t c)
{
	static int io_port[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };

	/* wait until serial port is ready */
	while (!(inb(io_port[comport] + 5) & 0x60));

	/* output character */
	outb(io_port[comport], c);
}


/**
 * Print null-terminated string to serial port
 */
inline void serial_out_string(Comport comport, const char *str)
{
	while (*str)
		serial_out_char(comport, *str++);
}


/**
 * Main program, called by the startup code in crt0.s
 */
extern "C" int _main(void)
{
	serial_out_string(COMPORT_0, "Hallo, this is some code running on OKL4.\n");
	serial_out_string(COMPORT_0, "Returning from main...\n");
	return 0;
}
