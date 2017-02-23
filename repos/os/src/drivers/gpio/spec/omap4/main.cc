/*
 * \brief  Gpio driver for the OMAP4
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
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
#include <driver.h>

Omap4_driver& Omap4_driver::factory(Genode::Env &env)
{
	static Omap4_driver driver(env);
	return driver;
}


struct Main
{
	Genode::Env         &env;
	Genode::Sliced_heap  sliced_heap;
	Omap4_driver        &driver;
	Gpio::Root           root;

	Genode::Attached_rom_dataspace config_rom { env, "config" };

	Main(Genode::Env &env)
	:
		env(env),
		sliced_heap(env.ram(), env.rm()),
		driver(Omap4_driver::factory(env)),
		root(&env.ep().rpc_ep(), &sliced_heap, driver)
	{
		using namespace Genode;

		log("--- omap4 gpio driver ---");

		Gpio::process_config(config_rom.xml(), driver);

		/*
		 * Announce service
		 */
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
