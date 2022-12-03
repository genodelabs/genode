/*
 * \brief  Tool for querying information from a file system
 * \author Norman Feske
 * \date   2018-08-17
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/registry.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <util/avl_tree.h>
#include <os/reporter.h>
#include <os/vfs.h>

/* local includes */
#include <for_each_subdir_name.h>

namespace Fs_query {
	using namespace Genode;
	struct Watched_file;
	struct Watched_directory;
	struct Main;
	using Node_rwx = Vfs::Node_rwx;
}


struct Fs_query::Watched_file
{
	File_content::Path const _name;

	/**
	 * Support for 'sorted_for_each'
	 */
	bool higher(Watched_file const &other) const
	{
		return (strcmp(other._name.string(), _name.string()) > 0);
	}

	Node_rwx const _rwx;

	Constructible<Watcher> _watcher { };

	Watched_file(Directory const &dir, File_content::Path name, Node_rwx rwx,
	             Vfs::Watch_response_handler &handler)
	:
		_name(name), _rwx(rwx)
	{
		if (_rwx.readable)
			_watcher.construct(dir, name, handler);
	}

	virtual ~Watched_file() { }

	void _gen_content(Xml_generator &xml, Allocator &alloc, Directory const &dir) const
	{
		File_content content(alloc, dir, _name, File_content::Limit{4*1024});

		bool content_is_xml = false;

		content.xml([&] (Xml_node node) {
			if (!node.has_type("empty")) {
				xml.attribute("xml", "yes");
				xml.append("\n");
				node.with_raw_node([&] (char const *start, size_t length) {
					xml.append(start, length); });
				content_is_xml = true;
			}
		});

		if (!content_is_xml) {
			content.bytes([&] (char const *base, size_t len) {
				xml.append_sanitized(base, len); });
		}
	}

	void gen_query_response(Xml_generator &xml, Xml_node query,
	                        Allocator &alloc, Directory const &dir) const
	{
		try {
			xml.node("file", [&] () {
				xml.attribute("name", _name);

				if (query.attribute_value("size", false))
					xml.attribute("size", dir.file_size(_name));

				if (_rwx.writeable)
					xml.attribute("writeable", "yes");

				if (_rwx.readable)
					if (query.attribute_value("content", false))
						_gen_content(xml, alloc, dir);
			});
		}
		/*
		 * File may have disappeared since last traversal. This condition
		 * is detected on the attempt to obtain the file content.
		 */
		catch (Directory::Nonexistent_file) {
			warning("could not obtain content of nonexistent file ", _name); }
		catch (File::Open_failed) {
			warning("cannot open file ", _name, " for reading"); }
		catch (File::Truncated_during_read) {
			warning("file ", _name, " truncated during read"); }
		catch (Xml_generator::Buffer_exceeded) { throw; }
	}
};


struct Fs_query::Watched_directory
{
	Allocator &_alloc;

	Directory::Path const _rel_path;

	Directory const _dir;

	Watcher _watcher;

	Registry<Registered<Watched_file> > _files { };

	Watched_directory(Allocator &alloc, Directory &other, Directory::Path const &rel_path,
	                  Vfs::Watch_response_handler &handler)
	:
		_alloc(alloc), _rel_path(rel_path),
		_dir(other, rel_path), _watcher(other, rel_path, handler)
	{
		_dir.for_each_entry([&] (Directory::Entry const &entry) {

			using Dirent_type = Vfs::Directory_service::Dirent_type;
			bool const file = (entry.type() == Dirent_type::CONTINUOUS_FILE)
			               || (entry.type() == Dirent_type::TRANSACTIONAL_FILE);
			if (file) {
				try {
					new (_alloc) Registered<Watched_file>(_files, _dir, entry.name(),
					                                      entry.rwx(), handler);
				} catch (...) { }
			}
		});
	}

	virtual ~Watched_directory()
	{
		_files.for_each([&] (Registered<Watched_file> &file) {
			destroy(_alloc, &file); });
	}

	bool has_name(Directory::Path const &name) const { return _rel_path == name; }

	void gen_query_response(Xml_generator &xml, Xml_node query) const
	{
		xml.node("dir", [&] () {
			xml.attribute("path", _rel_path);

			for_each_subdir_name(_alloc, _dir, [&] (Directory::Entry::Name const &name) {
				xml.node("dir", [&] () {
					xml.attribute("name", name); }); });

			sorted_for_each(_alloc, _files, [&] (Watched_file const &file) {
				file.gen_query_response(xml, query, _alloc, _dir); });
		});
	}
};


struct Fs_query::Main : Vfs::Watch_response_handler
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Vfs::Global_file_system_factory _fs_factory { _heap };

	/**
	 * Vfs::Watch_response_handler interface
	 */
	void watch_response() override
	{
		Signal_transmitter(_config_handler).submit();
	}

	Vfs::Simple_env _vfs_env { _env, _heap, _config.xml().sub_node("vfs") };

	Directory _root_dir { _vfs_env };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Expanding_reporter _reporter { _env, "listing", "listing" };

	Registry<Registered<Watched_directory> > _dirs { };

	void _gen_listing(Xml_generator &xml, Xml_node config) const
	{
		config.for_each_sub_node("query", [&] (Xml_node query) {
			Directory::Path const path = query.attribute_value("path", Directory::Path());
			_dirs.for_each([&] (Watched_directory const &dir) {
				if (dir.has_name(path))
					dir.gen_query_response(xml, query);
			});
		});
	}

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		_vfs_env.root_dir().apply_config(config.sub_node("vfs"));

		_dirs.for_each([&] (Registered<Watched_directory> &dir) {
			destroy(_heap, &dir); });

		config.for_each_sub_node("query", [&] (Xml_node query) {
			Directory::Path const path = query.attribute_value("path", Directory::Path());
			try {
				new (_heap)
					Registered<Watched_directory>(
						_dirs, _heap, _root_dir, path, *this);
			}
			catch (Genode::Directory::Nonexistent_directory) { }
		});

		_reporter.generate([&] (Xml_generator &xml) {
			_gen_listing(xml, config); });
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env)
{
	static Fs_query::Main main(env);
}

