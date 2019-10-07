/*
 * \brief  Tool for performing a sequence of file operations
 * \author Norman Feske
 * \date   2019-03-12
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/sleep.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/vfs.h>

namespace Fs_tool {
	using namespace Genode;
	struct Main;
}


struct Fs_tool::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Vfs::Global_file_system_factory _fs_factory { _heap };

	struct Vfs_env : Vfs::Env
	{
		Main &_main;

		Vfs_env(Main &main) : _main(main) { }

		Genode::Env                 &env()           override { return _main._env; }
		Allocator                   &alloc()         override { return _main._heap; }
		Vfs::File_system            &root_dir()      override { return _main._root_dir_fs; }

	} _vfs_env { *this };

	Vfs::Dir_file_system _root_dir_fs {
		_vfs_env, _config.xml().sub_node("vfs"), _fs_factory };

	Directory _root_dir { _vfs_env };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	bool _verbose = false;

	typedef Directory::Path Path;

	void _remove_file(Xml_node);

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		_verbose = config.attribute_value("verbose", false);

		_root_dir_fs.apply_config(config.sub_node("vfs"));

		config.for_each_sub_node([&] (Xml_node operation) {
			if (operation.has_type("remove-file")) {
				_remove_file(operation);
			}
		});

		if (config.attribute_value("exit", false)) {
			_env.parent().exit(0);
			sleep_forever();
		}
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Fs_tool::Main::_remove_file(Xml_node operation)
{
	Path const path = operation.attribute_value("path", Path());

	if (!_root_dir.file_exists(path)) {

		if (_verbose) {
			if (_root_dir.directory_exists(path))
				warning("file", path, " cannot be removed because it is a directory");
			else
				warning("file", path, " cannot be removed because there is no such file");
		}
		return;
	}

	if (_verbose)
		log("remove file ", path);

	_root_dir.unlink(path);

	if (_verbose && _root_dir.file_exists(path))
		warning("failed to remove file ", path);
}


void Component::construct(Genode::Env &env) { static Fs_tool::Main main(env); }

