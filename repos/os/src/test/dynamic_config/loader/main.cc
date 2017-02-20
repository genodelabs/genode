/*
 * \brief  Test for changing the configuration of a loader plugin at runtime
 * \author Christian Prochaska
 * \date   2012-04-20
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <loader_session/connection.h>
#include <base/component.h>

using namespace Genode;

struct Main
{
	enum { CONFIG_SIZE = 100 };

	Env                  &env;
	int                   counter       { -1 };
	Loader::Connection    loader        { env, 8 * 1024 * 1024 };
	Timer::Connection     timer         { env };
	Signal_handler<Main>  timer_handler { env.ep(), *this, &Main::handle_timer };

	void handle_timer()
	{
		char *config_ds_addr =
			env.rm().attach(loader.alloc_rom_module("config", CONFIG_SIZE));

		String<100> config("<config><counter>", counter++, "</counter></config>");
		strncpy(config_ds_addr, config.string(), CONFIG_SIZE);
		env.rm().detach(config_ds_addr);
		loader.commit_rom_module("config");
		timer.trigger_once(250 * 1000);
	}

	Main(Env &env) : env(env)
	{
		timer.sigh(timer_handler);
		handle_timer();
		loader.start("test-dynamic_config", "test-label");
	}
};

void Component::construct(Env &env) { static Main main(env); }
