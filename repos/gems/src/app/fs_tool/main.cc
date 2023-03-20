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
	struct Byte_buffer;
	struct Main;
}


struct Fs_tool::Byte_buffer : Byte_range_ptr
{
	Allocator &_alloc;

	Byte_buffer(Allocator &alloc, size_t size)
	: Byte_range_ptr((char *)alloc.alloc(size), size), _alloc(alloc) { }

	~Byte_buffer() { _alloc.free(start, num_bytes); }
};


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

	void _copy_file(Path const &from, Path const &to, Byte_range_ptr const &);

	void _remove_file    (Xml_node const &);
	void _new_file       (Xml_node const &);
	void _copy_all_files (Xml_node const &);

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

			if (operation.has_type("copy-all-files"))
				_copy_all_files(operation);
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


void Fs_tool::Main::_remove_file(Xml_node const &operation)
{
	Path const path = operation.attribute_value("path", Path());

	if (!_root_dir.file_exists(path)) {

		if (_verbose) {
			if (_root_dir.directory_exists(path))
				warning("file ", path, " cannot be removed because it is a directory");
			else
				warning("file ", path, " cannot be removed because there is no such file");
		}
		return;
	}

	if (_verbose)
		log("remove file ", path);

	_root_dir.unlink(path);

	if (_verbose && _root_dir.file_exists(path))
		warning("failed to remove file ", path);
}


void Fs_tool::Main::_new_file(Xml_node const &operation)
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


void Fs_tool::Main::_copy_file(Path const &from, Path const &to,
                               Byte_range_ptr const &buffer)
{
	try {
		Readonly_file const src { _root_dir, from };
		New_file            dst { _root_dir, to };

		Readonly_file::At at { 0 };

		for (;;) {

			size_t const read_bytes = src.read(at, buffer);

			dst.append(buffer.start, read_bytes);

			at.value += read_bytes;

			if (read_bytes < buffer.num_bytes)
				break;
		}
	}
	catch (...) {
		error("failed to copy ", from, " to ", to); }
}


void Fs_tool::Main::_copy_all_files(Xml_node const &operation)
{
	Number_of_bytes const default_buffer { 1024*1024 };
	Byte_buffer buffer(_heap, operation.attribute_value("buffer", default_buffer));

	Path const from = operation.attribute_value("from", Path());
	Path const to   = operation.attribute_value("to",   Path());

	if (!_root_dir.directory_exists(from))
		return;

	Directory(_root_dir, from).for_each_entry([&] (Directory::Entry const &entry) {

		bool const continous_file =
			(entry.type() == Vfs::Directory_service::Dirent_type::CONTINUOUS_FILE);

		if (continous_file)
			_copy_file(Path(from, "/", entry.name()),
			           Path(to,   "/", entry.name()),
			           buffer);
	});
}


void Component::construct(Genode::Env &env) { static Fs_tool::Main main(env); }

