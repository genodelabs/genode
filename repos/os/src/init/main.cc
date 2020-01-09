/*
 * \brief  Init component
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/sandbox.h>

namespace Init {

	using namespace Genode;

	struct Main;
}


struct Init::Main
{
	Env &_env;

	Sandbox _sandbox { _env };

	Attached_rom_dataspace _config { _env, "config" };

	void _handle_resource_avail() { }

	Signal_handler<Main> _resource_avail_handler {
		_env.ep(), *this, &Main::_handle_resource_avail };

	void _handle_config()
	{
		_config.update();
		_sandbox.apply_config(_config.xml());
	}

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);

		/* prevent init to block for resource upgrades (never satisfied by core) */
		_env.parent().resource_avail_sigh(_resource_avail_handler);

		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Init::Main main(env); }

