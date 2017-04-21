/*
 * \brief  Bump-in-the-wire component to dump NIC traffic info to the log
 * \author Martin Stein
 * \date   2017-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>

/* local includes */
#include <component.h>
#include <uplink.h>

using namespace Net;
using namespace Genode;


class Main
{
	private:

		Genode::Attached_rom_dataspace _config;
		Timer::Connection              _timer;
		unsigned                       _curr_time { 0 };
		Genode::Heap                   _heap;
		Uplink                         _uplink;
		Net::Root                      _root;

	public:

		Main(Env &env);
};


Main::Main(Env &env)
:
	_config(env, "config"), _timer(env), _heap(&env.ram(), &env.rm()),
	_uplink(env, _config.xml(), _timer, _curr_time, _heap),
	_root(env.ep(), _heap, _uplink, env.ram(), _config.xml(), _timer,
	      _curr_time, env.rm())
{
	env.parent().announce(env.ep().manage(_root));
}


void Component::construct(Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main main(env);
}
