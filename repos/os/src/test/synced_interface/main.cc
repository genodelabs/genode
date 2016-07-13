/*
 * \brief  Test for 'Synced_interface'
 * \author Norman Feske
 * \date   2013-05-16
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/synced_interface.h>
#include <base/log.h>


struct Adder
{
	int add(int a, int b)
	{
		Genode::log("adding ", a, " + ", b);
		return a + b;
	}
};


struct Pseudo_lock
{
	void lock()   { Genode::log("lock"); }
	void unlock() { Genode::log("unlock"); }
};


int main(int, char **)
{
	using namespace Genode;

	Pseudo_lock lock;
	Adder       adder;

	Synced_interface<Adder, Pseudo_lock> synced_adder(lock, &adder);

	int const res = synced_adder()->add(13, 14);

	log("result is ", res);
	return 0;
}
