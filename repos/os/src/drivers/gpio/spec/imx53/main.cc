/*
 * \brief  Gpio driver for the i.MX53
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-12-09
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <gpio/component.h>
#include <gpio/config.h>
#include <os/server.h>

/* local includes */
#include <driver.h>


struct Main
{
	Server::Entrypoint   &ep;
	Genode::Sliced_heap   sliced_heap;
	Imx53_driver         &driver;
	Gpio::Root            root;

	Main(Server::Entrypoint &ep)
	:
		ep(ep),
		sliced_heap(Genode::env()->ram_session(), Genode::env()->rm_session()),
		driver(Imx53_driver::factory(ep)),
		root(&ep.rpc_ep(), &sliced_heap, driver)
	{
		using namespace Genode;

		log("--- i.MX53 gpio driver ---");

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
	char const *name()             { return "gpio_drv_ep";        }
	size_t stack_size()            { return 16*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);      }
}
