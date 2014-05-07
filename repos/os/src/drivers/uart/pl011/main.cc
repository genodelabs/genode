/*
 * \brief  Driver for PL011 UARTs
 * \author Christian Helmuth
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <pl011_defs.h>
#include <os/config.h>
#include <cap_session/connection.h>

/* local includes */
#include "pl011.h"
#include "uart_component.h"


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- PL011 UART driver started ---\n");

	/**
	 * Factory used by 'Uart::Root' at session creation/destruction time
	 */
	struct Pl011_driver_factory : Uart::Driver_factory
	{
		Pl011 *created[PL011_NUM];

		/**
		 * Constructor
		 */
		Pl011_driver_factory()
		{
			for (unsigned i = 0; i < PL011_NUM; i++)
				created[i] = 0;
		}

		Uart::Driver *create(unsigned index, unsigned /* baudrate */,
		                     Uart::Char_avail_callback &callback)
		{
			PINF("Setting baudrate is not supported yet. Use default 115200.");
			/*
			 * We assume the underlying kernel uses UART0 and, therefore, start at
			 * index 1 for the user-level driver.
			 */
			if (index < 1 || index >= PL011_NUM)
				throw Uart::Driver_factory::Not_available();

			Pl011_uart *cfg  = &pl011_uart[index];
			Pl011      *uart =  created[index];

			if (!uart) {
				uart = new (env()->heap())
				       Pl011(cfg->mmio_base, cfg->mmio_size,
				             PL011_IBRD_115200, PL011_FBRD_115200,
				             cfg->irq_number, callback);

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
