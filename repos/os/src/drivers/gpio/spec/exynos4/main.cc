/*
 * \brief  Gpio driver for the Odroid-x2
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopéz Leon <humberto@uclv.cu>
 * \author Reinier Millo Sánchez <rmillo@uclv.cu>
 * \date   2015-07-03
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
#include <base/heap.h>
#include <base/log.h>
#include <gpio/component.h>
#include <gpio/config.h>

/* local includes */
#include <driver.h>


Gpio::Odroid_x2_driver& Gpio::Odroid_x2_driver::factory(Genode::Env &env)
{
	static Odroid_x2_driver driver(env);
	return driver;
}


struct Main
{
	Genode::Env            &env;
	Genode::Sliced_heap     sliced_heap;
	Gpio::Odroid_x2_driver &driver;
	Gpio::Root              root;

	Genode::Attached_rom_dataspace config_rom { env, "config" };

	Main(Genode::Env &env)
	: env(env),
	  sliced_heap(env.ram(), env.rm()),
	  driver(Gpio::Odroid_x2_driver::factory(env)),
	  root(&env.ep().rpc_ep(), &sliced_heap, driver)
	{
		using namespace Genode;

		log("--- Odroid_x2 gpio driver ---");

		Gpio::process_config(config_rom.xml(), driver);

		/*
		 * Announce service
		 */
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
