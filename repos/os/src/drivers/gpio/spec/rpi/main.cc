/*
 * \brief  Gpio driver for the RaspberryPI
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopéz Leon <humberto@uclv.cu>
 * \date   2015-07-23
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <gpio/component.h>
#include <gpio/config.h>
#include <os/server.h>

/* local includes */
#include "driver.h"

struct Main
{
	Server::Entrypoint   &ep;
	Genode::Sliced_heap  sliced_heap;
	Gpio::Rpi_driver     &driver;
	Gpio::Root           root;

	Main(Server::Entrypoint &ep)
	:
		ep(ep),
		sliced_heap(Genode::env()->ram_session(), Genode::env()->rm_session()),
		driver(Gpio::Rpi_driver::factory(ep)),
		root(&ep.rpc_ep(), &sliced_heap, driver)
	{
		using namespace Genode;
		log("--- Raspberry Pi GPIO driver ---");

		/*
		 * Check configuration for async events detect
		 */
		unsigned int async = 0;
		try {
		config()->xml_node().attribute("async_events").value(&async);
		} catch (...) { }
		driver.set_async_events(async>0);

		/*
		 * Check for common GPIO configuration
		 */
		Gpio::process_config(driver);

		/*
		 * Check configuration for specific function
		 */
		try {
			Xml_node gpio_node = config()->xml_node().sub_node("gpio");

			for (;; gpio_node = gpio_node.next("gpio")) {
				unsigned num     = 0;
				unsigned function = 0;

				try {
					gpio_node.attribute("num").value(&num);
					gpio_node.attribute("function").value(&function);

					switch(function){
					case 0: driver.set_func(num, Gpio::Reg::FSEL_ALT0); break;
					case 1: driver.set_func(num, Gpio::Reg::FSEL_ALT1); break;
					case 2: driver.set_func(num, Gpio::Reg::FSEL_ALT2); break;
					case 3: driver.set_func(num, Gpio::Reg::FSEL_ALT3); break;
					case 4: driver.set_func(num, Gpio::Reg::FSEL_ALT4); break;
					case 5: driver.set_func(num, Gpio::Reg::FSEL_ALT5); break;
					default: warning("wrong pin function, ignore node");
					}
				} catch(Xml_node::Nonexistent_attribute) {
					warning("missing attribute, ignore node");
				}
				if (gpio_node.last("gpio")) break;
			}
		} catch (Xml_node::Nonexistent_sub_node) { warning("no GPIO config"); }

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
