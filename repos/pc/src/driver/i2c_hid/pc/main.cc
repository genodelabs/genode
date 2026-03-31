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
struct i2c_hid_config    i2c_hid_config;
struct i2c_master_config i2c_master_config;

static void init_i2c_hid_config(Node const &config)
{
	/* defaults for Fuji U7411 */
	i2c_hid_config.irq             = 266;
	i2c_hid_config.bus_addr        = 44;
	i2c_hid_config.hid_addr        = 32;
	i2c_master_config.bus_speed_hz = 400000;
	i2c_master_config.ss_hcnt      = 500;
	i2c_master_config.ss_lcnt      = 588;
	i2c_master_config.ss_ht        = 80;
	i2c_master_config.fs_hcnt      = 193;
	i2c_master_config.fs_lcnt      = 313;
	i2c_master_config.fs_ht        = 80;

	config.with_optional_sub_node("i2c_slave", [&] (Node const & node) {
		if (node.has_attribute("gpio_chip")) {
			i2c_hid_config.mode = Irq_mode::GPIO;

			String<32> name = node.attribute_value("gpio_chip", String<32> { "INT34C5" });
			copy_cstring(i2c_hid_config.gpiochip_name, name.string(), 32);
		} else
			i2c_hid_config.mode = Irq_mode::APIC;

		i2c_hid_config.irq      = node.attribute_value("irq",      i2c_hid_config.irq);
		i2c_hid_config.bus_addr = node.attribute_value("bus_addr", i2c_hid_config.bus_addr);
		i2c_hid_config.hid_addr = node.attribute_value("hid_addr", i2c_hid_config.hid_addr);
	});

	config.with_optional_sub_node("i2c_master", [&] (Node const & node) {
		i2c_master_config.bus_speed_hz =
			node.attribute_value("bus_speed_hz", i2c_master_config.bus_speed_hz);

		node.with_optional_sub_node("sscn", [&] (Node const & n) {
			i2c_master_config.ss_hcnt = n.attribute_value("hcnt",      i2c_master_config.ss_hcnt);
			i2c_master_config.ss_lcnt = n.attribute_value("lcnt",      i2c_master_config.ss_lcnt);
			i2c_master_config.ss_ht   = n.attribute_value("hold_time", i2c_master_config.ss_ht);
		});
		node.with_optional_sub_node("fmcn", [&] (Node const & n) {
			i2c_master_config.fs_hcnt = n.attribute_value("hcnt",      i2c_master_config.fs_hcnt);
			i2c_master_config.fs_lcnt = n.attribute_value("lcnt",      i2c_master_config.fs_lcnt);
			i2c_master_config.fs_ht   = n.attribute_value("hold_time", i2c_master_config.fs_ht);
		});
	});

	switch (i2c_hid_config.mode)
	{
		case Irq_mode::GPIO:
			log("using GPIO pin ", i2c_hid_config.irq, " via ",
			    Cstring(i2c_hid_config.gpiochip_name), " bus_addr=",
			    i2c_hid_config.bus_addr, " hid_addr=", i2c_hid_config.hid_addr);
			break;
		case Irq_mode::APIC:
			log("using IRQ ", i2c_hid_config.irq, " bus_addr=",
			    i2c_hid_config.bus_addr, " hid_addr=", i2c_hid_config.hid_addr);
			break;
	}
}


struct Main
{
	Env &env;

	Attached_rom_dataspace config { env, "config" };

	Signal_handler<Main> scheduler_handler { env.ep(), *this, &Main::handle_scheduler };

	Io_signal_handler<Main> heartbeat_handler { env.ep(), *this, &Main::handle_heartbeat };

	Main(Env & env);

	void handle_scheduler() { Lx_kit::env().scheduler.execute(); }

	void handle_heartbeat()
	{
		extern unsigned evdev_count;

		if (evdev_count)
			env.parent().heartbeat_response();
	}
};


Main::Main(Env &env) : env(env)
{
	config.update();
	init_i2c_hid_config(config.node());

	Lx_kit::initialize(env, scheduler_handler);
	env.exec_static_constructors();
	env.parent().heartbeat_sigh(heartbeat_handler);

	/* init C API */
	genode_event_init(genode_env_ptr(env),
	                  genode_allocator_ptr(Lx_kit::env().heap));

	lx_emul_start_kernel(nullptr);
}


void Component::construct(Env & env) { static Main main(env); }
