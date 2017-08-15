/*
 * \brief  Server component for Network Address Translation on NIC sessions
 * \author Martin Stein
 * \date   2016-08-24
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

		Timer::Connection              _timer;
		Genode::Heap                   _heap;
		Genode::Attached_rom_dataspace _config_rom;
		Configuration                  _config;
		Uplink                         _uplink;
		Net::Root                      _root;

	public:

		Main(Env &env);
};


Main::Main(Env &env)
:
	_timer(env), _heap(&env.ram(), &env.rm()), _config_rom(env, "config"),
	_config(_config_rom.xml(), _heap), _uplink(env, _timer, _heap, _config),
	_root(env.ep(), _timer, _heap, _uplink.router_mac(), _config,
	      env.ram(), env.rm())
{
	env.parent().announce(env.ep().manage(_root));
}


void Component::construct(Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	try { static Main main(env); }

	catch (Net::Domain_tree::No_match) {
		error("failed to find configuration for domain 'uplink'");
		env.parent().exit(-1);
	}
}
