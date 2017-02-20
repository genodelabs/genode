/*
 * \brief  Test for changing configuration at runtime
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-04-04
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>

using namespace Genode;

struct Main
{
	Env                    &env;
	Attached_rom_dataspace  config         { env, "config" };
	Signal_handler<Main>    config_handler { env.ep(), *this, &Main::handle_config };

	void handle_config()
	{
		config.update();
		try {
			long counter = 1;
			config.xml().sub_node("counter").value(&counter);
			log("obtained counter value ", counter, " from config");
		}
		catch (...) { error("could not parse configuration"); }
	}

	Main(Env &env) : env(env)
	{
		handle_config();
		config.sigh(config_handler);
	}
};

void Component::construct(Env &env) { static Main main(env); }
