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

namespace Net { class Main; }


class Net::Main
{
	private:

		Genode::Env                    &_env;
		Interface_list                  _interfaces     { };
		Timer::Connection               _timer          { _env };
		Genode::Heap                    _heap           { &_env.ram(), &_env.rm() };
		Genode::Attached_rom_dataspace  _config_rom     { _env, "config" };
		Reference<Configuration>        _config         { _init_config() };
		Signal_handler<Main>            _config_handler { _env.ep(), *this, &Main::_handle_config };
		Uplink                          _uplink         { _env, _timer, _heap, _interfaces, _config() };
		Root                            _root           { _env.ep(), _timer, _heap, _uplink.router_mac(), _config(), _env.ram(), _interfaces, _env.rm()};

		void _handle_config();

		Configuration &_init_config();

		template <typename FUNC>
		void _for_each_interface(FUNC && functor)
		{
			_interfaces.for_each([&] (Interface &interface) {
				functor(interface);
			});
			_config().domains().for_each([&] (Domain &domain) {
				domain.interfaces().for_each([&] (Interface &interface) {
					functor(interface);
				});
			});
		}

	public:

		Main(Env &env);
};


Configuration &Net::Main::_init_config()
{
	Configuration &config_legacy = *new (_heap)
		Configuration(_config_rom.xml(), _heap);

	Configuration &config = *new (_heap)
		Configuration(_env, _config_rom.xml(), _heap, _timer, config_legacy);

	destroy(_heap, &config_legacy);
	return config;
}


Net::Main::Main(Env &env) : _env(env)
{
	_config_rom.sigh(_config_handler);
	env.parent().announce(env.ep().manage(_root));
}


void Net::Main::_handle_config()
{
	_config_rom.update();
	Configuration &config = *new (_heap)
		Configuration(_env, _config_rom.xml(), _heap, _timer, _config());

	_root.handle_config(config);
	_for_each_interface([&] (Interface &intf) { intf.handle_config(config); });
	_for_each_interface([&] (Interface &intf) { intf.handle_config_aftermath(); });

	destroy(_heap, &_config());
	_config = Reference<Configuration>(config);
}


void Component::construct(Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	try { static Net::Main main(env); }

	catch (Net::Domain_tree::No_match) {
		error("failed to find configuration for domain 'uplink'");
		env.parent().exit(-1);
	}
}
