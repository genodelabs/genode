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

/* local includes */
#include "driver.h"


struct Main
{
	Genode::Env         &env;
	Genode::Sliced_heap  sliced_heap;
	Gpio::Rpi_driver     driver;
	Gpio::Root           root;

	Genode::Attached_rom_dataspace config_rom { env, "config" };

	Main(Genode::Env &env)
	:
		env(env),
		sliced_heap(env.ram(), env.rm()),
		driver(env),
		root(&env.ep().rpc_ep(), &sliced_heap, driver)
	{
		using namespace Genode;
		log("--- Raspberry Pi GPIO driver ---");

		Xml_node const config = config_rom.xml();

		/*
		 * Check configuration for async events detect
		 */
		unsigned int async = config.attribute_value("async_events", 0U);
		driver.set_async_events(async>0);

		/*
		 * Check for common GPIO configuration
		 */
		Gpio::process_config(config, driver);

		/*
		 * Check configuration for specific function
		 */
		if (!config.has_sub_node("gpio"))
			warning("no GPIO config");

		config_rom.xml().for_each_sub_node("gpio", [&] (Xml_node const &gpio_node) {

			unsigned const num      = gpio_node.attribute_value("num",      0U);
			unsigned const function = gpio_node.attribute_value("function", 0U);

			switch(function){
			case 0: driver.set_func(num, Gpio::Reg::FSEL_ALT0); break;
			case 1: driver.set_func(num, Gpio::Reg::FSEL_ALT1); break;
			case 2: driver.set_func(num, Gpio::Reg::FSEL_ALT2); break;
			case 3: driver.set_func(num, Gpio::Reg::FSEL_ALT3); break;
			case 4: driver.set_func(num, Gpio::Reg::FSEL_ALT4); break;
			case 5: driver.set_func(num, Gpio::Reg::FSEL_ALT5); break;
			default: warning("wrong pin function, ignore node");
			}
		});

		/*
		 * Announce service
		 */
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
