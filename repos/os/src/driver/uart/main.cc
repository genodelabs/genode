/*
 * \brief  Driver for UART devices
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/log.h>
#include <base/attached_io_mem_dataspace.h>

/* local includes */
#include <uart_component.h>

namespace Uart { struct Main; }


struct Uart::Main
{
	Env  &_env;
	Heap _heap { _env.ram(), _env.rm() };

	Uart::Driver_factory _factory { _env, _heap };

	Uart::Root _root { _env, _heap, _factory };

	Main(Env &env) : _env(env)
	{
		log("--- UART driver started ---");
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Uart::Main uart_drv(env); }


Uart::Driver & Uart::Driver_factory::create(unsigned index, unsigned baudrate,
                                            Uart::Char_avail_functor &functor)
{
	if (index >= UARTS_NUM) throw Not_available();

	if (!drivers[index])
		drivers[index] = new (&heap) Driver(env, index, baudrate, functor);
	return *drivers[index];
}
