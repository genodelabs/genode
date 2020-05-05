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
#include <base/sleep.h>
#include <root/component.h>
#include <timer_session/connection.h>
#include <log_session/connection.h>

namespace Dummy {

	struct Log_service;
	struct Log_connections;
	struct Ram_consumer;
	struct Cap_consumer;
	struct Resource_yield_handler;
	struct Main;
	using namespace Genode;
}


struct Dummy::Log_service
{
	Env &_env;

	Sliced_heap _heap { _env.ram(), _env.rm() };

	bool const _verbose;

	struct Session_component : Rpc_object<Log_session>
	{
		Session_label const _label;

		bool const _verbose;

		Session_component(Session_label const &label, bool verbose)
		:
			_label(label), _verbose(verbose)
		{
			if (_verbose)
				log("opening session with label ", _label);
		}

		~Session_component()
		{
			if (_verbose)
				log("closing session with label ", _label);
		}

		void write(String const &string) override
		{
			/* strip known line delimiter from incoming message */
			unsigned n = 0;
			Genode::String<16> const pattern("\033[0m\n");
			for (char const *s = string.string(); s[n] && pattern != s + n; n++);

			typedef Genode::String<100> Message;
			Message const message("[", _label, "] ", Cstring(string.string(), n));
			log(message);
		}
	};

	struct Root : Root_component<Session_component>
	{
		Pd_session &_pd;

		bool const _verbose;

		Root(Entrypoint &ep, Allocator &alloc, Pd_session &pd, bool verbose)
		:
			Root_component(ep, alloc), _pd(pd), _verbose(verbose)
		{ }

		Session_component *_create_session(const char *args, Affinity const &) override
		{
			return new (md_alloc()) Session_component(label_from_args(args), _verbose);
		}

		void _upgrade_session(Session_component *, const char *args) override
		{
			size_t const ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			if (_pd.avail_ram().value >= ram_quota)
				log("received session quota upgrade");
		}
	};

	Root _root { _env.ep(), _heap, _env.pd(), _verbose };

	Log_service(Env &env, bool verbose) : _env(env), _verbose(verbose)
	{
		_env.parent().announce(_env.ep().manage(_root));
		log("created LOG service");
	}

	~Log_service() { _env.ep().dissolve(_root); }
};


struct Dummy::Log_connections
{
	Env &_env;

	Sliced_heap _heap { _env.ram(), _env.rm() };

	typedef Registered<Log_connection> Connection;

	Registry<Connection> _connections { };

	Log_connections(Env &env, Xml_node node) : _env(env)
	{
		unsigned const count = node.attribute_value("count", 0UL);

		Number_of_bytes const ram_upgrade =
			node.attribute_value("ram_upgrade", Number_of_bytes());

		log("going to create ", count, " LOG connections");

		for (unsigned i = 0; i < count; i++) {

			Connection *connection =
				new (_heap) Connection(_connections, _env, Session_label { i });

			if (ram_upgrade > 0) {
				log("upgrade connection ", i);
				connection->upgrade_ram(ram_upgrade);
			}
		}

		log("created all LOG connections");
	}

	~Log_connections()
	{
		_connections.for_each([&] (Connection &c) { destroy(_heap, &c); });

		log("destroyed all LOG connections");
	}
};


struct Dummy::Ram_consumer
{
	size_t _amount = 0;

	Ram_dataspace_capability _ds_cap { };

	Ram_allocator &_ram;

	Ram_consumer(Ram_allocator &ram) : _ram(ram) { }

	void release()
	{
		if (!_amount)
			return;

		log("release ", Number_of_bytes(_amount), " bytes of memory");
		_ram.free(_ds_cap);

		_ds_cap = Ram_dataspace_capability();
		_amount = 0;
	}

	void consume(size_t amount)
	{
		if (_amount)
			release();

		log("consume ", Number_of_bytes(amount), " bytes of memory");
		_ds_cap = _ram.alloc(amount);
		_amount = amount;
	}
};


struct Dummy::Cap_consumer
{
	Entrypoint &_ep;

	size_t _amount = 0;

	struct Interface : Genode::Interface { GENODE_RPC_INTERFACE(); };

	struct Object : Genode::Rpc_object<Interface>
	{
		Entrypoint &_ep;
		Object(Entrypoint &ep) : _ep(ep) { _ep.manage(*this); }
		~Object() { _ep.dissolve(*this); }
	};

	/*
	 * Statically allocate an array of RPC objects to avoid RAM allocations
	 * as side effect during the cap-consume step.
	 */
	static constexpr size_t MAX = 100;

	Constructible<Object> _objects[MAX];

	Cap_consumer(Entrypoint &ep) : _ep(ep) { }

	void release()
	{
		if (!_amount)
			return;

		log("release ", _amount, " caps");
		for (unsigned i = 0; i < MAX; i++)
			_objects[i].destruct();

		_amount = 0;
	}

	void consume(size_t amount)
	{
		if (_amount)
			release();

		log("consume ", amount, " caps");
		for (unsigned i = 0; i < min(amount, MAX); i++)
			_objects[i].construct(_ep);

		_amount = amount;
	}
};


struct Dummy::Resource_yield_handler
{
	Env &_env;

	Ram_consumer &_ram_consumer;
	Cap_consumer &_cap_consumer;

	void _handle_yield()
	{
		log("got yield request");
		_ram_consumer.release();
		_cap_consumer.release();
		_env.parent().yield_response();
	}

	Signal_handler<Resource_yield_handler> _yield_handler {
		_env.ep(), *this, &Resource_yield_handler::_handle_yield };

	Resource_yield_handler(Env &env,
	                       Ram_consumer &ram_consumer, Cap_consumer &cap_consumer)
	:
		_env(env), _ram_consumer(ram_consumer), _cap_consumer(cap_consumer)
	{
		_env.parent().yield_sigh(_yield_handler);
	}
};


struct Dummy::Main
{
	Env &_env;

	Constructible<Timer::Connection> _timer { };

	Attached_rom_dataspace _config { _env, "config" };

	unsigned _config_cnt = 0;

	typedef String<50> Version;

	Version _config_version { };

	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	Ram_consumer _ram_consumer { _env.ram() };
	Cap_consumer _cap_consumer { _env.ep()  };

	Constructible<Resource_yield_handler> _resource_yield_handler { };

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
				_log_service.construct(_env, node.attribute_value("verbose", false));

			if (node.type() == "consume_ram")
				_ram_consumer.consume(node.attribute_value("amount", Number_of_bytes()));

			if (node.type() == "consume_caps")
				_cap_consumer.consume(node.attribute_value("amount", 0UL));

			if (node.type() == "handle_yield_requests")
				_resource_yield_handler.construct(_env, _ram_consumer, _cap_consumer);

			if (node.type() == "sleep") {

				if (!_timer.constructed())
					_timer.construct(_env);

				_timer->msleep(node.attribute_value("ms", (uint64_t)100));
			}

			if (node.type() == "sleep_forever")
				sleep_forever();

			if (node.type() == "log")
				log(node.attribute_value("string", String<50>()));

			if (node.type() == "exit") {
				_env.parent().exit(0);
				sleep_forever();
			}
		});
	}

	Constructible<Log_connections> _log_connections { };

	Constructible<Log_service> _log_service { };

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Dummy::Main main(env); }
