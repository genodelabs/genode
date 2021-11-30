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
	env.devices.update(env.config.xml());
	root.update_policy();
}

void Component::construct(Genode::Env &env) {
	static Driver::Main main(env); }
