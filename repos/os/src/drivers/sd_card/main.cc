/*
 * \brief  SD-card driver
 * \author Martin Stein
 * \author Sebastian Sumpf
 * \date   2013-03-06
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <block/component.h>

/* local includes */
#include <driver.h>

using namespace Genode;

struct Main
{
	Env  &env;
	Heap  heap { env.ram(), env.rm() };

	struct Factory : Block::Driver_factory
	{
		Env  &env;
		Heap &heap;

		Factory(Env &env, Heap &heap) : env(env), heap(heap) { }

		Block::Driver *create() {
			return new (&heap) Sd_card::Driver(env); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap, static_cast<Sd_card::Driver*>(driver)); }

	} factory { env, heap };

	Block::Root root { env.ep(), heap, env.rm(), factory, true };

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- SD card driver ---");
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
