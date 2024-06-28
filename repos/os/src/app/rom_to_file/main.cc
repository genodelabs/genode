/*
 * \brief  Write content of ROM module to File_system
 * \author Johannes Schlatow
 * \date   2016-03-12
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/vfs.h>


namespace Rom_to_file {

	using namespace Genode;

	struct Main;
}


struct Rom_to_file::Main
{
	Env  &_env;
	Heap  _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config_rom { _env, "config" };

	Root_directory _root_dir { _env, _heap, _config_rom.xml().sub_node("vfs") };

	Constructible<Attached_rom_dataspace> _rom_ds { };

	using Rom_name = String<100>;

	/**
	 * Name of currently requested ROM module
	 *
	 * Solely used to detect configuration changes.
	 */
	Rom_name _rom_name { };

	/**
	 * Signal handler that is invoked when the configuration or the ROM module
	 * changes.
	 */
	void _handle_update();

	Signal_handler<Main> _update_dispatcher {
		_env.ep(), *this, &Main::_handle_update };

	Main(Env &env) : _env(env)
	{
		_config_rom.sigh(_update_dispatcher);
		_handle_update();
	}
};


void Rom_to_file::Main::_handle_update()
{
	_config_rom.update();

	Xml_node config = _config_rom.xml();

	/*
	 * Query name of ROM module from config
	 */
	Rom_name rom_name;
	try {
		rom_name = config.attribute_value("rom", rom_name);

	} catch (...) {
		warning("could not determine ROM name from config");
		return;
	}

	/*
	 * If ROM name changed, reconstruct '_rom_ds'
	 */
	if (rom_name != _rom_name) {
		_rom_ds.construct(_env, rom_name.string());
		_rom_ds->sigh(_update_dispatcher);
		_rom_name = rom_name;
	}

	/*
	 * Update ROM module and print content to LOG
	 */
	if (_rom_ds.constructed()) {
		_rom_ds->update();

		if (_rom_ds->valid()) {

			try {
				New_file new_file { _root_dir, _rom_name };

				size_t const len = max(strlen(_rom_ds->local_addr<char>()),
				                       _rom_ds->size());

				new_file.append(_rom_ds->local_addr<char>(), len);

			} catch (...) {
				error("cannot create file ", _rom_name);
				throw;
			}
		} else {
			log("ROM '", _rom_name, "' is invalid");
		}
	}
}


void Component::construct(Genode::Env &env) { static Rom_to_file::Main main(env); }
