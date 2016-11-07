/*
 * \brief  Gpio driver for the Odroid-x2
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopéz Leon <humberto@uclv.cu>
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-07-03
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/log.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <gpio/component.h>
#include <gpio/config.h>
#include <os/server.h>

/* local includes */
#include <driver.h>


struct Main
{
	Server::Entrypoint     &ep;
	Genode::Sliced_heap     sliced_heap;
	Gpio::Odroid_x2_driver &driver;
	Gpio::Root              root;

	Main(Server::Entrypoint &ep)
	: ep(ep),
	  sliced_heap(Genode::env()->ram_session(), Genode::env()->rm_session()),
	  driver(Gpio::Odroid_x2_driver::factory(ep)),
	  root(&ep.rpc_ep(), &sliced_heap, driver)
	{
		using namespace Genode;

		log("--- Odroid_x2 gpio driver ---");

		Gpio::process_config(driver);

		/*
		 * Announce service
		 */
		env()->parent()->announce(ep.manage(root));
	}
};

/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "gpio_drv_ep";     }
	size_t stack_size()            { return 1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);   }
}
