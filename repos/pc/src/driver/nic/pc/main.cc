/*
 * \brief  PC Ethernet driver
 * \author Christian Helmuth
 * \date   2023-05-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <lx_kit/init.h>
#include <lx_kit/env.h>
#include <lx_emul/init.h>
#include <lx_emul/task.h>
#include <genode_c_api/uplink.h>
#include <genode_c_api/mac_address_reporter.h>

namespace Pc {
	using namespace Genode;
	struct Main;
}


extern task_struct *user_task_struct_ptr;

struct Pc::Main
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	/**
	 * Config update signal handler
	 */
	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	void _handle_config()
	{
		_config.update();
		genode_mac_address_reporter_config(_config.xml());
	}

	/**
	 * Signal handler triggered by activity of the uplink connection
	 */
	Signal_handler<Main> _signal_handler { _env.ep(), *this, &Main::_handle_signal };

	void _handle_signal()
	{
		if (user_task_struct_ptr)
			lx_emul_task_unblock(user_task_struct_ptr);

		Lx_kit::env().scheduler.execute();

		genode_uplink_notify_peers();
	}

	Main(Env &env) : _env(env)
	{
		Lx_kit::initialize(env, _signal_handler);

		genode_mac_address_reporter_init(env, Lx_kit::env().heap);

		genode_uplink_init(genode_env_ptr(env),
		                   genode_allocator_ptr(Lx_kit::env().heap),
		                   genode_signal_handler_ptr(_signal_handler));

		/* subscribe to config updates and import initial config */
		_config.sigh(_config_handler);
		_handle_config();

		lx_emul_start_kernel(nullptr);
	}
};


void Component::construct(Genode::Env &env) { static Pc::Main main(env); }
