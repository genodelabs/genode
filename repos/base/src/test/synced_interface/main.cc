/*
 * \brief  Test for 'Synced_interface'
 * \author Norman Feske
 * \date   2013-05-16
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/synced_interface.h>
#include <base/component.h>

using namespace Genode;


struct Adder
{
	int add(int a, int b)
	{
		log("adding ", a, " + ", b);
		return a + b;
	}
};


struct Pseudo_lock
{
	void lock()   { log("lock"); }
	void unlock() { log("unlock"); }
};


struct Main
{
	Pseudo_lock                          lock;
	Adder                                adder;
	Synced_interface<Adder, Pseudo_lock> synced_adder { lock, &adder };

	Main(Env &env)
	{
		log("--- Synced interface test ---");
		int const res = synced_adder()->add(13, 14);
		log("result is ", res);
		log("--- Synced interface test finished ---");
	}
};


void Component::construct(Env &env) { static Main main(env); }
