/**
 * \brief  Block device encryption via cgd
 * \author Josef Soentgen
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
		Genode::Entrypoint &ep;
		Genode::Heap       &heap;

		Factory(Genode::Entrypoint &ep, Genode::Heap &heap)
		: ep(ep), heap(heap) { }

		Block::Driver *create() {
			return new (&heap) Driver(ep); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap, driver); }

	} factory { env.ep(), heap };

	Block::Root root { env.ep(), heap, factory };

	Main(Genode::Env &env) : env(env) {
		env.parent().announce(env.ep().manage(root)); }
};


void Component::construct(Genode::Env &env) { static Main inst(env); }
