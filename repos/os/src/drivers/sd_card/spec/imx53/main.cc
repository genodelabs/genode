/*
 * \brief  SD-card driver
 * \author Martin Stein
 * \date   2015-02-04
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
			return new (&heap) Block::Imx53_driver(true); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap,
			                static_cast<Block::Imx53_driver*>(driver)); }
	} factory { env.ep(), heap };

	Block::Root root { env.ep(), heap, factory  };

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- Imx53 SD card driver ---");

		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main m(env); }
