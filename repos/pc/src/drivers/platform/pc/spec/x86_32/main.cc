/*
 * \brief  Platform driver for PC
 * \author Stefan Kalkowski
 * \date   2022-10-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>

#include <common.h>

namespace Driver { struct Main; };

struct Driver::Main
{
	Env                  & _env;
	Attached_rom_dataspace _config_rom     { _env, "config"        };
	Attached_rom_dataspace _acpi_rom       { _env, "acpi"          };
	Attached_rom_dataspace _system_rom     { _env, "system"        };
	Common                 _common         { _env, _config_rom     };
	Signal_handler<Main>   _config_handler { _env.ep(), *this,
	                                         &Main::_handle_config };
	Signal_handler<Main>   _system_handler { _env.ep(), *this,
	                                         &Main::_system_update };

	void _handle_config();
	void _reset();
	void _system_update();

	Main(Genode::Env & e)
	: _env(e)
	{
		_config_rom.sigh(_config_handler);
		_acpi_rom.sigh(_system_handler);
		_system_rom.sigh(_system_handler);
		_handle_config();
		_system_update();
		_common.announce_service();
	}
};


void Driver::Main::_handle_config()
{
	_config_rom.update();
	_common.handle_config(_config_rom.xml());
}


void Driver::Main::_reset()
{
	_acpi_rom.update();
	_acpi_rom.xml().with_optional_sub_node("reset", [&] (Xml_node reset)
	{
		uint16_t const io_port = reset.attribute_value<uint16_t>("io_port", 0);
		uint8_t  const value   = reset.attribute_value<uint8_t>("value",    0);

		log("trigger reset by writing value ", value, " to I/O port ", Hex(io_port));

		try {
			Io_port_connection reset_port { _env, io_port, 1 };
			reset_port.outb(io_port, value);
		} catch (...) {
			error("unable to access reset I/O port ", Hex(io_port)); }
	});
}


void Driver::Main::_system_update()
{
	_system_rom.update();
	if (_system_rom.xml().attribute_value("state", String<16>()) == "reset")
		_reset();
}


void Component::construct(Genode::Env &env) {
	static Driver::Main main(env); }
