/*
 * \brief  I2C HID driver for PC-integrated devices
 * \author Christian Helmuth
 * \date   2022-05-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/env.h>
#include <base/attached_rom_dataspace.h>
#include <genode_c_api/event.h>

/* Lx_kit/emul includes */
#include <lx_emul/init.h>
#include <lx_kit/init.h>
#include <lx_kit/env.h>
#include <i2c_hid_config.h>

using namespace Genode;


/* global configuration attributes */
struct i2c_hid_config i2c_hid_config;

static void init_i2c_hid_config(Node const &config)
{
	i2c_hid_config.gpio_pin = config.attribute_value("gpio_pin", 266 /* 0x10a */);
	i2c_hid_config.bus_addr = config.attribute_value("bus_addr",  44 /*  0x2c */);
	i2c_hid_config.hid_addr = config.attribute_value("hid_addr",  32 /*  0x20 */);

	log("using gpio_pin=", i2c_hid_config.gpio_pin, " bus_addr=",
	    i2c_hid_config.bus_addr, " hid_addr=", i2c_hid_config.hid_addr);
}


struct Main
{
	Env &env;

	Attached_rom_dataspace config { env, "config" };

	Signal_handler<Main> scheduler_handler { env.ep(), *this, &Main::handle_scheduler };

	Main(Env & env);

	void handle_scheduler() { Lx_kit::env().scheduler.execute(); }
};


Main::Main(Env &env) : env(env)
{
	config.update();
	init_i2c_hid_config(config.node());

	Lx_kit::initialize(env, scheduler_handler);
	env.exec_static_constructors();

	/* init C API */
	genode_event_init(genode_env_ptr(env),
	                  genode_allocator_ptr(Lx_kit::env().heap));

	lx_emul_start_kernel(nullptr);
}


void Component::construct(Env & env) { static Main main(env); }
