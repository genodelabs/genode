/*
 * \brief  Driver for Exynos5 UART
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
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
#include "exynos5_uart.h"
#include "uart_component.h"


int main(int argc, char **argv)
{
	using namespace Genode;

	PINF("--- Exynos5 UART driver started ---\n");

	/**
	 * Factory used by 'Terminal::Root' at session creation/destruction time
	 */
	struct Exynos_uart_driver_factory : Uart::Driver_factory
	{
		Exynos_uart *created[UARTS_NUM];

		/**
		 * Constructor
		 */
		Exynos_uart_driver_factory()
		{
			for (unsigned i = 0; i < UARTS_NUM; i++)
				created[i] = 0;
		}

		Uart::Driver *create(unsigned index, unsigned baudrate,
								 Uart::Char_avail_callback &callback)
		{
			if (index >= UARTS_NUM)
				throw Uart::Driver_factory::Not_available();

			if (baudrate == 0)
			{
				PDBG("Baudrate is not defined. Use default 115200");
				baudrate = BAUD_115200;
			}

			Exynos_uart_cfg *cfg  = &exynos_uart_cfg[index];
			Exynos_uart     *uart =  created[index];

			Genode::Attached_io_mem_dataspace *uart_mmio = new (env()->heap())
				Genode::Attached_io_mem_dataspace(cfg->mmio_base, cfg->mmio_size);

			if (!uart) {
				uart = new (env()->heap())
					Exynos_uart(uart_mmio, cfg->irq_number, baudrate, callback);

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
