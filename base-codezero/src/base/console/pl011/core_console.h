/*
 * \brief  Console backend for PL011 UART on Codezero
 * \author Norman Feske
 * \date   2009-10-03
 *
 * This code assumes a PL011 UART as provided by 'qemu -M versatilepb'. Prior
 * executing this code, the kernel already initialized the UART to print some
 * startup message. So we can skip the UART initialization here. The kernel
 * maps the UART registers to the magic address PL011_BASE when starting mm0.
 * So we can just start using the device without any precautions.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/console.h>

/* codezero includes */
#include <codezero/syscalls.h>

typedef unsigned char uint8_t;

/**
 * Base address of default-mapped UART device
 *
 * defined in 'l4/arch/arm/io.h'
 */
enum { PL011_BASE = USERSPACE_CONSOLE_VBASE };

/**
 * UART registers
 */
enum { PL011_REG_UARTDR = PL011_BASE + 0x00 };
enum { PL011_REG_UARTFR = PL011_BASE + 0x18 };


/**
 * Returns true if UART is ready to transmit a character
 */
static bool pl011_tx_ready()
{
	enum { PL011_TX_FIFO_FULL = 1 << 5 };
	return !(*((volatile unsigned *)PL011_REG_UARTFR) & PL011_TX_FIFO_FULL);
}


/**
 * Output character to serial port
 */
static void pl011_out_char(uint8_t c)
{
	/* wait until serial port is ready */
	while (!pl011_tx_ready());

	/* output character */
	*((volatile unsigned int *)PL011_REG_UARTDR) = c;
}


namespace Genode
{
	class Core_console : public Console
	{
		protected:

			void _out_char(char c) {
				if(c == '\n')
					pl011_out_char('\r');
				pl011_out_char(c);
			}
	};
}

