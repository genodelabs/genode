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
#include <timer_session/connection.h>

/* local includes */
#include <nic_session_root.h>
#include <uplink_session_root.h>
#include <configuration.h>
#include <cached_timer.h>

using namespace Net;
using namespace Genode;

namespace Net { class Main; }


class Net::Main
{
	private:

		Genode::Env                    &_env;
		Quota                           _shared_quota        { };
		Interface_list                  _interfaces          { };
		Cached_timer                    _timer               { _env };
		Genode::Heap                    _heap                { &_env.ram(), &_env.rm() };
		Signal_handler<Main>            _report_handler      { _env.ep(), *this, &Main::_handle_report };
		Genode::Attached_rom_dataspace  _config_rom          { _env, "config" };
		Reference<Configuration>        _config              { *new (_heap) Configuration { _config_rom.xml(), _heap } };
		Signal_handler<Main>            _config_handler      { _env.ep(), *this, &Main::_handle_config };
		Nic_session_root                _nic_session_root    { _env, _timer, _heap, _config(), _shared_quota, _interfaces };
		Uplink_session_root             _uplink_session_root { _env, _timer, _heap, _config(), _shared_quota, _interfaces };

		void _handle_report();

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


void Main::_handle_report()
{
	try { _config().report().generate(); }
	catch (Pointer<Report>::Invalid) { }
	
}


Net::Main::Main(Env &env) : _env(env)
{
	_config_rom.sigh(_config_handler);
	_handle_config();
	env.parent().announce(env.ep().manage(_nic_session_root));
	env.parent().announce(env.ep().manage(_uplink_session_root));
}


void Net::Main::_handle_config()
{
	_config_rom.update();
	Configuration &old_config = _config();
	Configuration &new_config = *new (_heap)
		Configuration {
			_env, _config_rom.xml(), _heap, _report_handler, _timer,
			old_config, _shared_quota, _interfaces };

	_nic_session_root.handle_config(new_config);
	_uplink_session_root.handle_config(new_config);
	_for_each_interface([&] (Interface &intf) { intf.handle_config_1(new_config); });
	_for_each_interface([&] (Interface &intf) { intf.handle_config_2(); });
	_config = Reference<Configuration>(new_config);
	_for_each_interface([&] (Interface &intf) { intf.handle_config_3(); });

	destroy(_heap, &old_config);
}


void Component::construct(Env &env) { static Net::Main main(env); }
