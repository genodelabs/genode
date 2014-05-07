/*
 * \brief  Fiasco(.OC) KDB UART driver
 * \author Christian Prochaska
 * \date   2013-03-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/sleep.h>

#include <cap_session/connection.h>

#include "uart_component.h"

#include "kdb_uart.h"


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- Fiasco(.OC) KDB UART driver started ---\n");

	/**
	 * Factory used by 'Uart::Root' at session creation/destruction time
	 */
	struct Kdb_uart_driver_factory : Uart::Driver_factory
	{
		Kdb_uart *uart;

		/**
		 * Constructor
		 */
		Kdb_uart_driver_factory() : uart(0) { }

		Uart::Driver *create(unsigned index, unsigned baudrate,
		                     Uart::Char_avail_callback &callback)
		{
			/*
			 * We assume the underlying kernel uses UART0
			 */
			if (index > 0)
				throw Uart::Driver_factory::Not_available();

			if (!uart)
				uart = new (env()->heap()) Kdb_uart(callback);

			return uart;
		}

		void destroy(Uart::Driver *driver) { /* TODO */ }

	} driver_factory;

	enum { STACK_SIZE = 2*1024*sizeof(addr_t) };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "uart_ep");

	static Uart::Root uart_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&uart_root));

	sleep_forever();
	return 0;
}
