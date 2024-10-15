/*
 * \brief  C++ initialization, session, and client handling
 * \author Sebastian Sumpf
 * \date   2023-07-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>

#include <genode_c_api/uplink.h>
#include <genode_c_api/mac_address_reporter.h>

#include <nic_session/nic_session.h>

#include <lx_kit/env.h>
#include <lx_user/io.h>
#include <lx_emul/init.h>
#include <lx_emul/nic.h>

/* C-interface */
#include <usb_net.h>

using namespace Genode;

struct Main
{
	Env &env;

	Attached_rom_dataspace config_rom { env, "config" };

	unsigned long usb_config { 0 };

	Signal_handler<Main> signal_handler  { env.ep(), *this,
	                                       &Main::handle_signal  };
	Signal_handler<Main> usb_rom_handler { env.ep(), *this,
	                                       &Main::handle_usb_rom };
	Signal_handler<Main> config_handler  { env.ep(), *this,
	                                       &Main::handle_config  };

	Main(Env &env)
	:
		env(env)
	{
		Lx_kit::initialize(env, signal_handler);

		Genode_c_api::initialize_usb_client(env, Lx_kit::env().heap,
		                                    signal_handler, usb_rom_handler);

		genode_mac_address_reporter_init(env, Lx_kit::env().heap);

		genode_uplink_init(genode_env_ptr(env),
		                   genode_allocator_ptr(Lx_kit::env().heap),
		                   genode_signal_handler_ptr(signal_handler));

		config_rom.sigh(config_handler);
		handle_config();

		lx_emul_start_kernel(nullptr);
	}

	void handle_signal()
	{
		lx_user_handle_io();
		Lx_kit::env().scheduler.execute();
		genode_uplink_notify_peers();
	}

	void handle_usb_rom()
	{
		lx_emul_usb_client_rom_update();
		Lx_kit::env().scheduler.execute();
	}

	void handle_config()
	{
		config_rom.update();
		genode_mac_address_reporter_config(config_rom.xml());

		/* read USB configuration setting */
		usb_config = config_rom.xml().attribute_value("configuration", 0ul);

		/* retrieve possible MAC */
		if (config_rom.xml().has_attribute("mac")) {
			auto const mac = config_rom.xml().attribute_value("mac", Nic::Mac_address{});
			log("Trying to use configured mac: ", mac);
			lx_emul_nic_set_mac_address(mac.addr, sizeof(mac.addr));
		}
	}
};


void Component::construct(Env & env) { static Main main { env }; }
