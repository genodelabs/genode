/*
 * \brief  SD-card driver for Raspberry Pi
 * \author Norman Feske
 * \date   2014-09-21
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <platform_session/connection.h>

/* local includes */
#include <driver.h>


struct Main
{
	Genode::Env         &env;
	Genode::Heap         heap { env.ram(), env.rm() };
	Platform::Connection platform { env };

	struct Factory : Block::Driver_factory
	{
		Genode::Entrypoint &ep;
		Genode::Heap       &heap;

		Factory(Genode::Entrypoint &ep, Genode::Heap &heap)
		: ep(ep), heap(heap) { }

		Block::Driver *create() {
			return new (&heap) Block::Sdhci_driver(false); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap,
			                static_cast<Block::Sdhci_driver*>(driver)); }
	} factory { env.ep(), heap };

	Block::Root root { env.ep(), heap, factory  };

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- SD card driver ---");

		while (platform.power_state(Platform::Session::POWER_SDHCI) == 0)
			platform.power_state(Platform::Session::POWER_SDHCI, true);

		env.parent().announce(env.ep().manage(root));
	}
};


Genode::size_t Component::stack_size()      { return 2*1024*sizeof(long); }
void Component::construct(Genode::Env &env) { static Main m(env);         }
