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
#include <base/component.h>

/* local includes */
#include "block_driver.h"
#include "cgd.h"


struct Main
{
	Genode::Env &env;
	Genode::Heap heap { env.ram(), env.rm() };

	struct Factory : Block::Driver_factory
	{
		Genode::Env  &env;
		Genode::Heap &heap;

		Factory(Genode::Env &env, Genode::Heap &heap)
		: env(env), heap(heap) { }

		Block::Driver *create() {
			return new (&heap) Driver(env, heap); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap, driver); }

	} factory { env, heap };

	Block::Root root { env.ep(), heap, env.rm(), factory };

	Main(Genode::Env &env) : env(env) {
		env.parent().announce(env.ep().manage(root)); }
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main inst(env);
}
