/*
 * \brief  Platform driver - compound object for all derivate implementations
 * \author Stefan Kalkowski
 * \date   2022-05-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <root.h>

namespace Driver { class Common; };

class Driver::Common
{
	private:

		Env                    & _env;
		Attached_rom_dataspace   _devices_rom  { _env, "devices"          };
		Reporter                 _cfg_reporter { _env, "config"           };
		Reporter                 _dev_reporter { _env, "devices"          };
		Heap                     _heap         { _env.ram(), _env.rm()    };
		Sliced_heap              _sliced_heap  { _env.ram(), _env.rm()    };
		Device_model             _devices      { _heap, _dev_reporter     };
		Signal_handler<Common>   _dev_handler  { _env.ep(), *this,
		                                         &Common::_handle_devices };
		Driver::Root             _root;

		void _handle_devices();

	public:

		Heap         & heap()    { return _heap;    }
		Device_model & devices() { return _devices; }

		void handle_config(Xml_node config);
		void announce_service();

		Common(Genode::Env            & env,
		       Attached_rom_dataspace & config_rom);
};


void Driver::Common::_handle_devices()
{
	_devices_rom.update();
	_devices.update(_devices_rom.xml());
	_root.update_policy();
}


void Driver::Common::handle_config(Xml_node config)
{
	config.for_each_sub_node("report", [&] (Xml_node const node) {
		_dev_reporter.enabled(node.attribute_value("devices", false));
		_cfg_reporter.enabled(node.attribute_value("config",  false));
	});

	_root.update_policy();

	if (_cfg_reporter.enabled()) {
		Reporter::Xml_generator xml(_cfg_reporter, [&] () {
			config.with_raw_content([&] (char const *src, size_t len) {
				xml.append(src, len);
			});
		});
	}
}


void Driver::Common::announce_service()
{
	_env.parent().announce(_env.ep().manage(_root));
}


Driver::Common::Common(Genode::Env            & env,
                       Attached_rom_dataspace & config_rom)
:
	_env(env),
	_root(_env, _sliced_heap, config_rom, _devices)
{
	_handle_devices();
	_devices_rom.sigh(_dev_handler);
}
