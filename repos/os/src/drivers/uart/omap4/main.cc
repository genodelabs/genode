/*
 * \brief  Driver for OMAP4 UARTs
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2012-11-08
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <os/config.h>
#include <cap_session/connection.h>
#include <os/attached_io_mem_dataspace.h>

#include <uart_defs.h>

/* local includes */
#include "omap_uart.h"
#include "uart_component.h"


int main(int argc, char **argv)
{
	using namespace Genode;

	PDBG("--- OMAP4 UART driver started ---\n");

	/**
	 * Factory used by 'Terminal::Root' at session creation/destruction time
	 */
	struct Omap_uart_driver_factory : Uart::Driver_factory
	{
		Omap_uart *created[UARTS_NUM];

		/**
		 * Constructor
		 */
		Omap_uart_driver_factory()
		{
			for (unsigned i = 0; i < UARTS_NUM; i++)
				created[i] = 0;
		}

		Uart::Driver *create(unsigned index, unsigned baudrate,
		                     Uart::Char_avail_callback &callback)
		{
			if (index > UARTS_NUM)
				throw Uart::Driver_factory::Not_available();
			
			if (baudrate == 0)
			{
				PDBG("Baudrate is not defined. Use default 115200");
				baudrate = BAUD_115200;
			}

			Omap_uart_cfg *cfg  = &omap_uart_cfg[index];
			Omap_uart     *uart =  created[index];

			Genode::Attached_io_mem_dataspace *uart_mmio = new (env()->heap())
				Genode::Attached_io_mem_dataspace(cfg->mmio_base, cfg->mmio_size);

			if (!uart) {
				uart = new (env()->heap())
					Omap_uart(uart_mmio, cfg->irq_number, baudrate, callback);

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
