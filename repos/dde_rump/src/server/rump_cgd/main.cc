/**
 * \brief  Block device encryption via cgd
 * \author Josef Soentgen
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <os/server.h>

/* local includes */
#include "block_driver.h"
#include "cgd.h"


struct Main
{
	Genode::Env &env;
	Genode::Heap heap { env.ram(), env.rm() };

	struct Factory : Block::Driver_factory
	{
		Genode::Entrypoint  &ep;
		Genode::Ram_session &ram;
		Genode::Heap        &heap;

		Factory(Genode::Entrypoint &ep, Genode::Ram_session &ram, Genode::Heap &heap)
		: ep(ep), ram(ram), heap(heap) { }

		Block::Driver *create() {
			return new (&heap) Driver(ep, ram); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap, driver); }

	} factory { env.ep(), env.ram(), heap };

	Block::Root root { env.ep(), heap, env.rm(), factory };

	Main(Genode::Env &env) : env(env) {
		env.parent().announce(env.ep().manage(root)); }
};


void Component::construct(Genode::Env &env) { static Main inst(env); }
