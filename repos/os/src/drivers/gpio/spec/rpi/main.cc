/*
 * \brief  Gpio driver for the RaspberryPI
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopéz Leon <humberto@uclv.cu>
 * \date   2015-07-23
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <gpio/component.h>
#include <gpio/config.h>
#include <os/server.h>

/* local includes */
#include "driver.h"


Gpio::Rpi_driver& Gpio::Rpi_driver::factory(Genode::Env &env)
{
	static Rpi_driver driver(env);
	return driver;
}


struct Main
{
	Genode::Env         &env;
	Genode::Sliced_heap  sliced_heap;
	Gpio::Rpi_driver     &driver;
	Gpio::Root           root;

	Genode::Attached_rom_dataspace config_rom { env, "config" };

	Main(Genode::Env &env)
	:
		env(env),
		sliced_heap(env.ram(), env.rm()),
		driver(Gpio::Rpi_driver::factory(env)),
		root(&env.ep().rpc_ep(), &sliced_heap, driver)
	{
		using namespace Genode;
		log("--- Raspberry Pi GPIO driver ---");

		/*
		 * Check configuration for async events detect
		 */
		unsigned int async = 0;
		try {
			config_rom.xml().attribute("async_events").value(&async);
		} catch (...) { }
		driver.set_async_events(async>0);

		/*
		 * Check for common GPIO configuration
		 */
		Gpio::process_config(config_rom.xml(), driver);

		/*
		 * Check configuration for specific function
		 */
		try {
			Xml_node gpio_node = config_rom.xml().sub_node("gpio");

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
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
