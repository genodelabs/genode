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
#include <rom_session/connection.h>
#include <base/signal.h>
#include <base/attached_rom_dataspace.h>
#include <util/print_lines.h>
#include <util/reconstructible.h>

#include <base/heap.h>
#include <base/allocator_avl.h>

#include <file_system_session/connection.h>
#include <file_system/util.h>


namespace Rom_to_file {

	using namespace Genode;

	struct Main;

	enum  {
		BLOCK_SIZE   = 512,
		QUEUE_SIZE   = File_system::Session::TX_QUEUE_SIZE,
		TX_BUF_SIZE  = BLOCK_SIZE * (QUEUE_SIZE*2 + 1)
	};
}


struct Rom_to_file::Main
{
	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	Allocator_avl _alloc { &_heap };

	File_system::Connection _fs { _env, _alloc, "", "/", true, TX_BUF_SIZE };

	Constructible<Attached_rom_dataspace> _rom_ds { };

	typedef Genode::String<100> Rom_name;

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

	Main(Genode::Env &env) : _env(env)
	{
		_config_rom.sigh(_update_dispatcher);
		_handle_update();
	}
};


void Rom_to_file::Main::_handle_update()
{
	_config_rom.update();

	Genode::Xml_node config = _config_rom.xml();

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
			using namespace File_system;

			char dir_path[] = "/";
			const char *file_name = _rom_name.string();

			try {
				Dir_handle   dir_handle = ensure_dir(_fs, dir_path);
				Handle_guard dir_guard(_fs, dir_handle);
				Constructible<File_handle> handle;

				try {
					handle.construct(_fs.file(dir_handle, file_name, File_system::WRITE_ONLY, true));
				} catch (Node_already_exists) {
					handle.construct(_fs.file(dir_handle, file_name, File_system::WRITE_ONLY, false));
				}

				_fs.truncate(*handle, 0);

				size_t len     = max(strlen(_rom_ds->local_addr<char>()), _rom_ds->size());
				size_t written = write(_fs, *handle, _rom_ds->local_addr<void>(), len, 0);

				if (written < len) {
					warning(written, " of ", len, " bytes have been written");
				}

				_fs.close(*handle);

			} catch (Permission_denied) {
				error(Cstring(dir_path), file_name, ": permission denied");

			} catch (No_space) {
				error("file system out of space");

			} catch (Out_of_ram) {
				error("server ran out of RAM");

			} catch (Out_of_caps) {
				error("server ran out of caps");

			} catch (Invalid_name) {
				error(Cstring(dir_path), file_name, ": invalid path");

			} catch (Name_too_long) {
				error(Cstring(dir_path), file_name, ": name too long");

			} catch (...) {
				error("cannot open file ", Cstring(dir_path), file_name);
				throw;
			}
		} else {
			log("ROM '", _rom_name, "' is invalid");
		}
	}
}


void Component::construct(Genode::Env &env) { static Rom_to_file::Main main(env); }
