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
#include <base/buffered_output.h>

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

	Vfs::Simple_env _vfs_env { _env, _heap, _config.xml().sub_node("vfs") };

	Directory _root_dir { _vfs_env };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	bool _verbose = false;

	typedef Directory::Path Path;

	void _remove_file(Xml_node);

	void _new_file(Xml_node);

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		_verbose = config.attribute_value("verbose", false);

		_vfs_env.root_dir().apply_config(config.sub_node("vfs"));

		config.for_each_sub_node([&] (Xml_node operation) {

			if (operation.has_type("remove-file"))
				_remove_file(operation);

			if (operation.has_type("new-file"))
				_new_file(operation);
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


void Fs_tool::Main::_new_file(Xml_node operation)
{
	Path const path { operation.attribute_value("path", Path()) };

	bool write_error  = false;
	bool create_error = false;

	try {
		New_file new_file(_root_dir, path);
		auto write = [&] (char const *str)
		{
			if (new_file.append(str, strlen(str)) != New_file::Append_result::OK)
				write_error = true;
		};
		Buffered_output<128, decltype(write)> output(write);

		operation.with_raw_content([&] (char const *start, size_t size) {
			print(output, Cstring(start, size)); });
	}
	catch (New_file::Create_failed) {
		create_error = true; }

	if (create_error && _verbose)
		warning("operation <new-file path=\"", path, "\"> "
		        "failed because creating the file failed");

	if (write_error && _verbose)
		warning("operation <new-file path=\"", path, "\"> "
		        "failed because writing to the file failed");
}


void Component::construct(Genode::Env &env) { static Fs_tool::Main main(env); }

