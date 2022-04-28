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
	Heap                   heap         { env.ram(), env.rm() };
	Sliced_heap            sliced_heap  { env.ram(), env.rm() };
	Attached_rom_dataspace config_rom   { env, "config"       };
	Attached_rom_dataspace devices_rom  { env, "devices"      };
	Reporter               cfg_reporter { env, "config"       };
	Reporter               dev_reporter { env, "devices"      };
	Device_model           devices      { heap, dev_reporter  };
	Signal_handler<Main>   handler      { env.ep(), *this,
	                                      &Main::update       };
	Driver::Root           root         { env, sliced_heap,
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

	config_rom.xml().for_each_sub_node("report", [&] (Xml_node const node) {
		dev_reporter.enabled(node.attribute_value("devices", false));
		cfg_reporter.enabled(node.attribute_value("config",  false));
	});

	devices.update(devices_rom.xml());
	root.update_policy();

	if (cfg_reporter.enabled()) {
		Reporter::Xml_generator xml(cfg_reporter, [&] () {
			config_rom.xml().with_raw_content([&] (char const *src, size_t len) {
				xml.append(src, len);
			});
		});
	}
}

void Component::construct(Genode::Env &env) {
	static Driver::Main main(env); }
