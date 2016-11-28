/*
 * \brief  eMMC driver for Arndale/Exynos5 platform
 * \author Sebastian Sumpf
 * \date   2013-03-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <regulator_session/connection.h>

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
			return new (&heap) Block::Exynos5_driver(ep, true); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap,
			                static_cast<Block::Exynos5_driver *>(driver)); }
	} factory { env.ep(), heap };

	Regulator::Connection regulator { env, Regulator::CLK_MMC0 };
	Block::Root           root      { env.ep(), heap, factory  };

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- Arndale eMMC card driver ---");

		env.parent().announce(env.ep().manage(root));
		regulator.state(true);
	}
};


void Component::construct(Genode::Env &env) { static Main m(env); }
