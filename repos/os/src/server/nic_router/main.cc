/*
 * \brief  Server component for Network Address Translation on NIC sessions
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/component.h>
#include <base/heap.h>
#include <os/config.h>
#include <os/timer.h>
#include <nic/xml_node.h>
#include <timer_session/connection.h>

/* local includes */
#include <component.h>
#include <uplink.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


class Main
{
	private:

		Timer::Connection _timer_connection;
		Genode::Timer     _timer;
		Genode::Heap      _heap;
		Configuration     _config;
		Uplink            _uplink;
		Net::Root         _root;

	public:

		Main(Env &env);
};


Main::Main(Env &env)
:
	_timer_connection(env), _timer(_timer_connection, env.ep()),
	_heap(&env.ram(), &env.rm()), _config(config()->xml_node(), _heap),
	_uplink(env.ep(), _timer, _heap, _config),
	_root(env.ep(), _timer, _heap, _uplink.router_mac(), _config, env.ram())
{
	env.parent().announce(env.ep().manage(_root));
}


void Component::construct(Env &env) { static Main main(env); }
