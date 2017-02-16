/*
 * \brief  Dummy component used for automated component-composition tests
 * \author Norman Feske
 * \date   2017-02-16
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/registry.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>
#include <log_session/connection.h>

namespace Dummy {

	struct Log_connections;
	struct Main;
	using namespace Genode;
}


struct Dummy::Log_connections
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	typedef Registered<Log_connection> Connection;

	Registry<Connection> _connections;

	Log_connections(Env &env, Xml_node node) : _env(env)
	{
		unsigned const count = node.attribute_value("count", 0UL);

		log("going to create ", count, " LOG connections");

		for (unsigned i = 0; i < count; i++)
			new (_heap) Connection(_connections, _env, Session_label { i });

		log("created all LOG connections");
	}

	~Log_connections()
	{
		_connections.for_each([&] (Connection &c) { destroy(_heap, &c); });

		log("destroyed all LOG connections");
	}
};


struct Dummy::Main
{
	Env &_env;

	Constructible<Timer::Connection> _timer;

	Attached_rom_dataspace _config { _env, "config" };

	Constructible<Log_connections> _log_connections;

	Main(Env &env) : _env(env)
	{
		_config.xml().for_each_sub_node([&] (Xml_node node) {

			if (node.type() == "create_log_connections")
				_log_connections.construct(_env, node);

			if (node.type() == "destroy_log_connections")
				_log_connections.destruct();

			if (node.type() == "sleep") {

				if (!_timer.constructed())
					_timer.construct(_env);

				_timer->msleep(node.attribute_value("ms", 100UL));
			}

			if (node.type() == "log")
				log(node.attribute_value("string", String<50>()));
		});
	}
};




void Component::construct(Genode::Env &env) { static Dummy::Main main(env); }
