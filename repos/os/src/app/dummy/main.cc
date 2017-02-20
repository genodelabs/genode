/*
 * \brief  Dummy component used for automated component-composition tests
 * \author Norman Feske
 * \date   2017-02-16
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/registry.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <root/component.h>
#include <timer_session/connection.h>
#include <log_session/connection.h>

namespace Dummy {

	struct Log_service;
	struct Log_connections;
	struct Main;
	using namespace Genode;
}


struct Dummy::Log_service
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	struct Session_component : Rpc_object<Log_session>
	{
		Session_label const _label;

		Session_component(Session_label const &label) : _label(label) { }

		size_t write(String const &string) override
		{
			/* strip known line delimiter from incoming message */
			unsigned n = 0;
			Genode::String<16> const pattern("\033[0m\n");
			for (char const *s = string.string(); s[n] && pattern != s + n; n++);

			typedef Genode::String<100> Message;
			Message const message("[", _label, "] ", Cstring(string.string(), n));
			log(message);

			return strlen(string.string());
		}
	};

	struct Root : Root_component<Session_component>
	{
		Root(Entrypoint &ep, Allocator &alloc) : Root_component(ep, alloc) { }

		Session_component *_create_session(const char *args, Affinity const &) override
		{
			return new (md_alloc()) Session_component(label_from_args(args));
		}
	};

	Root _root { _env.ep(), _heap };

	Log_service(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_root));
		log("created LOG service");
	}

	~Log_service() { _env.ep().dissolve(_root); }
};


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

	unsigned _config_cnt = 0;

	typedef String<50> Version;

	Version _config_version;

	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	void _handle_config()
	{
		_config.update();

		Version const version = _config.xml().attribute_value("version", Version());
		if (_config_cnt > 0 && version == _config_version)
			return;

		_config_cnt++;
		_config_version = version;

		if (_config_version.valid())
			log("config ", _config_cnt, ": ", _config_version);

		_config.xml().for_each_sub_node([&] (Xml_node node) {

			if (node.type() == "create_log_connections")
				_log_connections.construct(_env, node);

			if (node.type() == "destroy_log_connections")
				_log_connections.destruct();

			if (node.type() == "log_service")
				_log_service.construct(_env);

			if (node.type() == "sleep") {

				if (!_timer.constructed())
					_timer.construct(_env);

				_timer->msleep(node.attribute_value("ms", 100UL));
			}

			if (node.type() == "log")
				log(node.attribute_value("string", String<50>()));
		});
	}

	Constructible<Log_connections> _log_connections;

	Constructible<Log_service> _log_service;

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Dummy::Main main(env); }
