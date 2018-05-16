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
		Pointer<Uplink>                 _uplink         { };
		Root                            _root           { _env.ep(), _timer, _heap, _config(), _env.ram(), _interfaces, _env.rm()};

		void _handle_config();

		Configuration &_init_config();

		void _deinit_uplink(Configuration &config);

		void _init_uplink(Configuration       &config,
		                  Session_label const &label);

		void _uplink_handle_config(Configuration &config);

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
	_uplink_handle_config(_config());
	_config_rom.sigh(_config_handler);
	env.parent().announce(env.ep().manage(_root));
}


void Net::Main::_init_uplink(Configuration       &config,
                             Session_label const &label)
{
	if (config.verbose()) {
		log("[uplink] request NIC session \"", label, "\""); }

	Uplink &uplink = *new (_heap) Uplink(_env, _timer, _heap, _interfaces,
	                                     config, label);
	_uplink = Pointer<Uplink>(uplink);
	uplink.init();
}


void Net::Main::_deinit_uplink(Configuration &config)
{
	try {
		Uplink &uplink = _uplink();
		if (config.verbose()) {
			log("[uplink] close NIC session \"", uplink.label(), "\""); }

		destroy(_heap, &uplink);
		_uplink = Pointer<Uplink>();
	}
	catch (Pointer<Uplink>::Invalid) { }
}


void Net::Main::_uplink_handle_config(Configuration &config)
{
	try {
		Session_label const &label =
			config.domains().find_by_name("uplink").label();

		try {
			if (label == _uplink().label()) {
				return;
			}
			_deinit_uplink(config);
			_init_uplink(config, label);
		}
		catch (Pointer<Uplink>::Invalid) { _init_uplink(config, label); }
	}
	catch (Domain_tree::No_match) { _deinit_uplink(config); }
}


void Net::Main::_handle_config()
{
	_config_rom.update();
	Configuration &old_config = _config();
	Configuration &new_config = *new (_heap)
		Configuration(_env, _config_rom.xml(), _heap, _timer, old_config);

	_uplink_handle_config(new_config);
	_root.handle_config(new_config);
	_for_each_interface([&] (Interface &intf) { intf.handle_config_1(new_config); });
	_for_each_interface([&] (Interface &intf) { intf.handle_config_2(); });
	_config = Reference<Configuration>(new_config);
	_for_each_interface([&] (Interface &intf) { intf.handle_config_3(); });

	destroy(_heap, &old_config);
}


void Component::construct(Env &env)
{
	try { static Net::Main main(env); }

	catch (Net::Domain_tree::No_match) {
		error("failed to find configuration for domain 'uplink'");
		env.parent().exit(-1);
	}
}
