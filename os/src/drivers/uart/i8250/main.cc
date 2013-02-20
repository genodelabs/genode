/*
 * \brief  Driver for PC UARTs
 * \author Norman Feske
 * \date   2011-09-12
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/sleep.h>
#include <os/config.h>

#include <cap_session/connection.h>

#include "i8250.h"
#include "uart_component.h"


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- i8250 UART driver started ---\n");

	/**
	 * Factory used by 'Uart::Root' at session creation/destruction time
	 */
	struct I8250_driver_factory : Uart::Driver_factory
	{
		enum { UART_NUM = 4 };
		I8250 *created[UART_NUM];

		/**
		 * Return I/O port base for specified UART
		 */
		static unsigned io_port_base(int index)
		{
			unsigned port_base[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
			return port_base[index & 0x3];
		}

		/**
		 * Return irq number for specified UART
		 */
		static int irq_number(int index)
		{
			int irq[] = { 4, 3, 4, 3 };
			return irq[index & 0x3];
		}

		/**
		 * Constructor
		 */
		I8250_driver_factory()
		{
			for (unsigned i = 0; i < UART_NUM; i++)
				created[i] = 0;
		}

		Uart::Driver *create(unsigned index, unsigned baudrate,
		                     Uart::Char_avail_callback &callback)
		{
			/*
			 * We assume the underlying kernel uses UART0 and, therefore, start at
			 * index 1 for the user-level driver.
			 */
			if (index < 1 || index >= UART_NUM)
				throw Uart::Driver_factory::Not_available();

			enum { BAUD = 115200 };
			if (baudrate == 0)
			{
				PINF("Baudrate is not defined. Use default 115200");
				baudrate = BAUD;
			}

			I8250 *uart =  created[index];

			if (!uart) {
				uart = new (env()->heap())
				       I8250(io_port_base(index), irq_number(index),
				             baudrate, callback);

				/* update 'created' table */
				created[index] = uart;
			}

			return uart;
		}

		void destroy(Uart::Driver *driver) { /* TODO */ }

	} driver_factory;

	enum { STACK_SIZE = 0x2000 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "uart_ep");

	static Uart::Root uart_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&uart_root));

	sleep_forever();
	return 0;
}
