/*
 * \brief  Helps synchronizing the CBE manager to the CBE-driver initialization
 * \author Martin Stein
 * \date   2021-03-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/vfs.h>

namespace Truncate_file {

	class Main;
}

using namespace Truncate_file;
using namespace Genode;


class Truncate_file::Main
{
	private:

		Env                    &_env;
		Heap                    _heap   { _env.ram(), _env.rm() };
		Attached_rom_dataspace  _config { _env, "config" };
		Root_directory          _vfs    { _env, _heap, _config.xml().sub_node("vfs") };
		Vfs::File_system       &_fs     { _vfs.root_dir() };
		Directory::Path  const  _path   { _config.xml().attribute_value("path", Directory::Path { }) };
		Number_of_bytes  const  _size   { _config.xml().attribute_value("size", Number_of_bytes { }) };

	public:

		Main(Env &env);
};


/*************************
 ** Truncate_file::Main **
 *************************/

Main::Main(Env &env)
:
	_env { env }
{
	unsigned mode = Vfs::Directory_service::OPEN_MODE_WRONLY;

	Vfs::Directory_service::Stat stat { };
	if (_fs.stat(_path.string(), stat) != Vfs::Directory_service::STAT_OK) {
		mode |= Vfs::Directory_service::OPEN_MODE_CREATE;
	}

	Vfs::Vfs_handle *handle_ptr = nullptr;
	Vfs::Directory_service::Open_result const res =
		_fs.open(_path.string(), mode, &handle_ptr, _heap);

	if (res != Vfs::Directory_service::OPEN_OK || (handle_ptr == nullptr)) {

		error("failed to create file '", _path, "'");
		class Create_failed { };
		throw Create_failed { };
	}
	handle_ptr->fs().ftruncate(handle_ptr, _size);
	handle_ptr->ds().close(handle_ptr);
	_env.parent().exit(0);
}


/***********************
 ** Genode::Component **
 ***********************/

void Component::construct(Genode::Env &env)
{
	static Truncate_file::Main main { env };
}
