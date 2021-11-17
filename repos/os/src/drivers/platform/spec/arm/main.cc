/*
 * \brief  Platform driver for ARM
 * \author Stefan Kalkowski
 * \date   2020-04-12
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <env.h>
#include <root.h>

namespace Driver { struct Main; };

struct Driver::Main
{
	void update_config();

	Driver::Env          env;
	Signal_handler<Main> config_handler { env.env.ep(), *this,
	                                      &Main::update_config };
	Driver::Root         root           { env };

	Main(Genode::Env & e)
	: env(e)
	{
		env.devices.update(env.config.xml());
		env.config.sigh(config_handler);
		env.env.parent().announce(env.env.ep().manage(root));
	}
};


void Driver::Main::update_config()
{
	env.config.update();

	/**
	 * We must perform the policy update before updating the devices since
	 * the former may need to release devices and its corresponding Io_mem
	 * and Irq connections when closing a session that is no longer supposed
	 * to exist. For doing so, it must access the old device model.
	 */
	root.update_policy();
	env.devices.update(env.config.xml());
}

void Component::construct(Genode::Env &env) {
	static Driver::Main main(env); }
