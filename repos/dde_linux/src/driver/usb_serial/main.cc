/*
 * \brief  USB serial: C++ initialization, session, and client handling
 * \author Christian Helmuth
 * \date   2025-09-02
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/registry.h>

#include <lx_emul/init.h>
#include <lx_emul/task.h>
#include <lx_emul/usb_client.h>
#include <lx_user/io.h>
#include <lx_kit/env.h>

#include <genode_c_api/usb_client.h>
#include <genode_c_api/terminal.h>

using namespace Genode;

struct Main
{
	Env &env;

	Signal_handler<Main> signal_handler  { env.ep(), *this, &Main::handle_signal  };
	Signal_handler<Main> usb_rom_handler { env.ep(), *this, &Main::handle_usb_rom };

	Main(Env &env) : env(env)
	{
		Lx_kit::initialize(env, signal_handler);

		Genode_c_api::initialize_usb_client(env, Lx_kit::env().heap,
		                                    signal_handler, usb_rom_handler);

		genode_terminal_init(genode_env_ptr(env),
		                     genode_allocator_ptr(Lx_kit::env().heap),
		                     genode_signal_handler_ptr(signal_handler));

		lx_emul_start_kernel(nullptr);
	}

	void handle_signal()
	{
		lx_user_handle_io();
		Lx_kit::env().scheduler.execute();
	}

	void handle_usb_rom()
	{
		lx_emul_usb_client_rom_update();
		Lx_kit::env().scheduler.execute();
	}
};


void Component::construct(Env &env) { static Main main(env); }
