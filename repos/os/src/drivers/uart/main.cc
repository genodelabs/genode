/*
 * \brief  Driver for UART devices
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-05
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/log.h>
#include <os/attached_io_mem_dataspace.h>

/* local includes */
#include <uart_component.h>


struct Main
{
	Genode::Env         &env;
	Genode::Heap         heap      { env.ram(), env.rm() };
	Uart::Driver_factory factory   { env, heap           };
	Uart::Root           uart_root { env, heap, factory  };

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- UART driver started ---");
		env.parent().announce(env.ep().manage(uart_root));
	}
};


namespace Component {
	Genode::size_t stack_size() { return 2048*sizeof(Genode::addr_t); }
	void           construct(Genode::Env &env) { static Main uart_drv(env); }
}


Uart::Driver & Uart::Driver_factory::create(unsigned index, unsigned baudrate,
                                            Uart::Char_avail_functor &functor)
{
	if (index >= UARTS_NUM) throw Not_available();

	if (!drivers[index])
		drivers[index] = new (&heap) Driver(env, index, baudrate, functor);
	return *drivers[index];
}
