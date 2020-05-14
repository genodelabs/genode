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
#include <base/env.h>
#include <base/heap.h>

#include <root.h>

namespace Driver { struct Main; };

struct Driver::Main
{
	void update_config();

	Genode::Env                  & env;
	Genode::Heap                   heap           { env.ram(), env.rm()  };
	Genode::Sliced_heap            sliced_heap    { env.ram(), env.rm()  };
	Genode::Attached_rom_dataspace config         { env, "config"        };
	Genode::Signal_handler<Main>   config_handler { env.ep(), *this,
	                                                &Main::update_config };
	Driver::Device_model           devices        { heap, config.xml()   };
	Driver::Root                   root           { env, sliced_heap,
	                                                config, devices      };

	Main(Genode::Env &env)
	: env(env)
	{
		config.sigh(config_handler);
		env.parent().announce(env.ep().manage(root));
	}
};


void Driver::Main::update_config()
{
	config.update();
	devices.update(config.xml());
	root.update_policy();
}

void Component::construct(Genode::Env &env) {
	static Driver::Main main(env); }
