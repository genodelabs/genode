/*
 * \brief  Platform driver
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
#include <root.h>

namespace Driver { struct Main; };

struct Driver::Main
{
	void update();

	Env                  & env;
	Heap                   heap        { env.ram(), env.rm() };
	Sliced_heap            sliced_heap { env.ram(), env.rm() };
	Attached_rom_dataspace config_rom  { env, "config"       };
	Attached_rom_dataspace devices_rom { env, "devices"      };
	Device_model           devices     { heap                };
	Signal_handler<Main>   handler     { env.ep(), *this,
	                                     &Main::update       };
	Driver::Root           root        { env, sliced_heap,
	                                     config_rom, devices };

	Main(Genode::Env & e)
	: env(e)
	{
		update();
		config_rom.sigh(handler);
		devices_rom.sigh(handler);
		env.parent().announce(env.ep().manage(root));
	}
};


void Driver::Main::update()
{
	config_rom.update();
	devices_rom.update();
	devices.update(devices_rom.xml());
	root.update_policy();
}

void Component::construct(Genode::Env &env) {
	static Driver::Main main(env); }
