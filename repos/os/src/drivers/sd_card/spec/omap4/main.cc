/*
 * \brief  SD-card driver for OMAP4 platform
 * \author Norman Feske
 * \date   2012-07-03
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>

/* local includes */
#include <driver.h>


struct Main
{
	Genode::Env &env;
	Genode::Heap heap { env.ram(), env.rm() };

	struct Factory : Block::Driver_factory
	{
		Genode::Entrypoint &ep;
		Genode::Heap       &heap;

		Factory(Genode::Entrypoint &ep, Genode::Heap &heap)
		: ep(ep), heap(heap) { }

		Block::Driver *create() {
			return new (&heap) Block::Omap4_driver(true); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap,
			                static_cast<Block::Omap4_driver*>(driver)); }
	} factory { env.ep(), heap };

	Block::Root root { env.ep(), heap, factory  };

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- OMAP4 SD card driver ---");

		env.parent().announce(env.ep().manage(root));
	}
};


Genode::size_t Component::stack_size()      { return 2*1024*sizeof(long); }
void Component::construct(Genode::Env &env) { static Main m(env);         }
