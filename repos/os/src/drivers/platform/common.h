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

class Driver::Common : Device_reporter
{
	private:

		Env                    & _env;
		String<64>               _rom_name;
		Attached_rom_dataspace   _devices_rom  { _env, _rom_name.string() };
		Heap                     _heap         { _env.ram(), _env.rm()    };
		Sliced_heap              _sliced_heap  { _env.ram(), _env.rm()    };
		Device_model             _devices      { _heap, *this             };
		Signal_handler<Common>   _dev_handler  { _env.ep(), *this,
		                                         &Common::_handle_devices };
		Driver::Root             _root;

		Constructible<Expanding_reporter> _cfg_reporter { };
		Constructible<Expanding_reporter> _dev_reporter { };

		void _handle_devices();

	public:

		Common(Genode::Env                  & env,
		       Attached_rom_dataspace const & config_rom);

		Heap         & heap()    { return _heap;    }
		Device_model & devices() { return _devices; }

		void announce_service();
		void handle_config(Xml_node config);


		/*********************
		 ** Device_reporter **
		 *********************/

		void update_report() override;
};


void Driver::Common::_handle_devices()
{
	_devices_rom.update();
	_devices.update(_devices_rom.xml());
	update_report();
	_root.update_policy();
}


void Driver::Common::update_report()
{
	if (_dev_reporter.constructed())
		_dev_reporter->generate([&] (Xml_generator & xml) {
			_devices.generate(xml); });
}


void Driver::Common::handle_config(Xml_node config)
{
	config.for_each_sub_node("report", [&] (Xml_node const node) {
		_dev_reporter.conditional(node.attribute_value("devices", false),
		                          _env, "devices", "devices");
		_cfg_reporter.conditional(node.attribute_value("config", false),
		                          _env, "config", "config");
	});

	_root.update_policy();

	if (_cfg_reporter.constructed())
		_cfg_reporter->generate([&] (Xml_generator & xml) {
			config.with_raw_content([&] (char const *src, size_t len) {
				xml.append(src, len);
			});
		});
}


void Driver::Common::announce_service()
{
	_env.parent().announce(_env.ep().manage(_root));
}


Driver::Common::Common(Genode::Env                  & env,
                       Attached_rom_dataspace const & config_rom)
:
	_env(env),
	_rom_name(config_rom.xml().attribute_value("devices_rom",
	                                           String<64>("devices"))),
	_root(_env, _sliced_heap, config_rom, _devices)
{
	_devices_rom.sigh(_dev_handler);
	_handle_devices();
}
