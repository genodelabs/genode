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
		Quota                           _shared_quota   { };
		Interface_list                  _interfaces     { };
		Timer::Connection               _timer          { _env };
		Genode::Heap                    _heap           { &_env.ram(), &_env.rm() };
		Genode::Attached_rom_dataspace  _config_rom     { _env, "config" };
		Reference<Configuration>        _config         { *new (_heap) Configuration { _config_rom.xml(), _heap } };
		Signal_handler<Main>            _config_handler { _env.ep(), *this, &Main::_handle_config };
		Root                            _root           { _env, _timer, _heap, _config(), _shared_quota, _interfaces };

		void _handle_config();

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


Net::Main::Main(Env &env) : _env(env)
{
	_config_rom.sigh(_config_handler);
	_handle_config();
	env.parent().announce(env.ep().manage(_root));
}


void Net::Main::_handle_config()
{
	_config().stop_reporting();

	_config_rom.update();
	Configuration &old_config = _config();
	Configuration &new_config = *new (_heap)
		Configuration(_env, _config_rom.xml(), _heap, _timer, old_config,
		              _shared_quota, _interfaces);

	_root.handle_config(new_config);
	_for_each_interface([&] (Interface &intf) { intf.handle_config_1(new_config); });
	_for_each_interface([&] (Interface &intf) { intf.handle_config_2(); });
	_config = Reference<Configuration>(new_config);
	_for_each_interface([&] (Interface &intf) { intf.handle_config_3(); });

	destroy(_heap, &old_config);

	_config().start_reporting();
}


void Component::construct(Env &env) { static Net::Main main(env); }
