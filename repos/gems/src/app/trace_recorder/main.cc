/*
 * \brief  Record traces and store in file system
 * \author Johannes Schlatow
 * \date   2022-05-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <monitor.h>

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>

namespace Trace_recorder {
	using namespace Genode;

	struct Main;
}


class Trace_recorder::Main
{
	private:
		Env                    &_env;

		Heap                    _heap           { _env.ram(), _env.rm() };
		Monitor                 _monitor        { _env, _heap };

		Attached_rom_dataspace  _config_rom     { _env, "config" };

		Signal_handler<Main>    _config_handler { _env.ep(), *this, &Main::_handle_config };

		bool                    _enabled        { false };

		void _handle_config();

	public:

		Main(Env & env)
		: _env(env)
		{
			_config_rom.sigh(_config_handler);

			_handle_config();
		}
};


void Trace_recorder::Main::_handle_config()
{
	_config_rom.update();

	bool old_enabled { _enabled };

	_enabled = _config_rom.xml().attribute_value("enable", false);

	if (old_enabled == _enabled) {
		warning("Config update postponed. Need to toggle 'enable' attribute.");
		return;
	}

	if (_enabled)
		_monitor.start(_config_rom.xml());
	else
		_monitor.stop();
}


void Component::construct(Genode::Env &env) { static Trace_recorder::Main main(env); }
