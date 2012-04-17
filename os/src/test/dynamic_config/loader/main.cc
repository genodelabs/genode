/*
 * \brief  Test for changing the configuration of a loader plugin at runtime
 * \author Christian Prochaska
 * \date   2012-04-20
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/snprintf.h>
#include <base/sleep.h>
#include <timer_session/connection.h>
#include <loader_session/connection.h>


using namespace Genode;


enum { CONFIG_SIZE = 100 };

static Loader::Connection loader(8*1024*1024);


static void update_config(int counter)
{
	Dataspace_capability config_ds =
		loader.alloc_rom_module("config", CONFIG_SIZE);

	char *config_ds_addr = env()->rm_session()->attach(config_ds);

	snprintf(config_ds_addr, CONFIG_SIZE,
	         "<config><counter>%d</counter></config>",
	         counter);

	env()->rm_session()->detach(config_ds_addr);

	loader.commit_rom_module("config");
}


int main(int, char **)
{
	update_config(-1);

	loader.start("test-dynamic_config", "test-label");

	/* update slave config at regular intervals */
	int counter = 0;
	for (;;) {

		static Timer::Connection timer;
		timer.msleep(250);
		update_config(counter++);
	}

	sleep_forever();

	return 0;
}
