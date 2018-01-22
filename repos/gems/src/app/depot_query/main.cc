/*
 * \brief  Tool for querying subsystem information from a depot
 * \author Norman Feske
 * \date   2017-07-04
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <gems/vfs.h>
#include <depot/archive.h>

namespace Depot_query {
	using namespace Depot;
	struct Recursion_limit;
	struct Dependencies;
	struct Main;
}


class Depot_query::Recursion_limit : Noncopyable
{
	public:

		class Reached : Exception { };

	private:

		unsigned const _value;

		static unsigned _checked_decr(unsigned value)
		{
			if (value == 0)
				throw Reached();

			return value - 1;
		}

	public:

		/**
		 * Constructor
		 */
		explicit Recursion_limit(unsigned value) : _value(value) { }

		/**
		 * Copy constructor
		 *
		 * \throw Recursion_limit::Reached
		 */
		Recursion_limit(Recursion_limit const &other)
		: _value(_checked_decr(other._value)) { }
};


/**
 * Collection of dependencies
 *
 * This data structure keeps track of a list of archive paths along with the
 * information of whether or not the archive is present in the depot. It also
 * ensures that all entries are unique.
 */
class Depot_query::Dependencies
{
	private:

		struct Collection : Noncopyable
		{
			Allocator &_alloc;

			typedef Registered_no_delete<Archive::Path> Entry;

			Registry<Entry> _entries;

			Collection(Allocator &alloc) : _alloc(alloc) { }

			~Collection()
			{
				_entries.for_each([&] (Entry &e) { destroy(_alloc, &e); });
			}

			bool known(Archive::Path const &path) const
			{
				bool result = false;
				_entries.for_each([&] (Entry const &entry) {
					if (path == entry)
						result = true; });

				return result;
			}

			void insert(Archive::Path const &path)
			{
				if (!known(path))
					new (_alloc) Entry(_entries, path);
			}

			template <typename FN>
			void for_each(FN const &fn) const { _entries.for_each(fn); };
		};

		Directory const &_depot;

		Collection _present;
		Collection _missing;

	public:

		Dependencies(Allocator &alloc, Directory const &depot)
		:
			_depot(depot), _present(alloc), _missing(alloc)
		{ }

		bool known(Archive::Path const &path) const
		{
			return _present.known(path) || _missing.known(path);
		}

		void record(Archive::Path const &path)
		{
			if (_depot.directory_exists(path))
				_present.insert(path);
			else
				_missing.insert(path);
		}

		void xml(Xml_generator &xml) const
		{
			_present.for_each([&] (Archive::Path const &path) {
				xml.node("present", [&] () { xml.attribute("path", path); }); });

			_missing.for_each([&] (Archive::Path const &path) {
				xml.node("missing", [&] () { xml.attribute("path", path); }); });
		}
};


struct Depot_query::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Constructible<Attached_rom_dataspace> _query_rom { };

	Root_directory _root { _env, _heap, _config.xml().sub_node("vfs") };

	Directory _depot_dir { _root, "depot" };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Reporter _directory_reporter    { _env, "directory"    };
	Reporter _blueprint_reporter    { _env, "blueprint"    };
	Reporter _dependencies_reporter { _env, "dependencies" };
	Reporter _user_reporter         { _env, "user"         };

	typedef String<64> Rom_label;
	typedef String<16> Architecture;

	Architecture _architecture;

	/**
	 * Look up ROM module 'rom_label' in the archives referenced by 'pkg_path'
	 *
	 * \throw Directory::Nonexistent_directory
	 * \throw Directory::Nonexistent_file
	 * \throw File::Truncated_during_read
	 * \throw Recursion_limit::Reached
	 */
	Archive::Path _find_rom_in_pkg(Directory::Path const &pkg_path,
	                               Rom_label       const &rom_label,
	                               Recursion_limit        recursion_limit);

	void _scan_depot_user_pkg(Archive::User const &, Directory const &, Xml_generator &);
	void _query_blueprint(Directory::Path const &, Xml_generator &);
	void _collect_source_dependencies(Archive::Path const &, Dependencies &, Recursion_limit);
	void _collect_binary_dependencies(Archive::Path const &, Dependencies &, Recursion_limit);
	void _query_user(Archive::User const &, Xml_generator &);

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		/*
		 * Depending of the 'query' config attribute, we obtain the query
		 * information from a separate ROM session (attribute value "rom")
		 * or from the depot_querty '<config>'.
		 */
		bool const query_from_rom =
			(config.attribute_value("query", String<5>()) == "rom");

		if (query_from_rom && !_query_rom.constructed()) {
			_query_rom.construct(_env, "query");
			_query_rom->sigh(_config_handler);
		}

		if (!query_from_rom && _query_rom.constructed())
			_query_rom.destruct();

		if (query_from_rom)
			_query_rom->update();

		Xml_node const query = (query_from_rom ? _query_rom->xml() : config);

		_directory_reporter   .enabled(query.has_sub_node("scan"));
		_blueprint_reporter   .enabled(query.has_sub_node("blueprint"));
		_dependencies_reporter.enabled(query.has_sub_node("dependencies"));
		_user_reporter        .enabled(query.has_sub_node("user"));

		_root.apply_config(config.sub_node("vfs"));

		if (!query.has_attribute("arch"))
			warning("query lacks 'arch' attribute");

		_architecture = query.attribute_value("arch", Architecture());

		if (_directory_reporter.enabled()) {
			Reporter::Xml_generator xml(_directory_reporter, [&] () {
				query.for_each_sub_node("scan", [&] (Xml_node node) {
					Archive::User const user = node.attribute_value("user", Archive::User());
					Directory::Path path("depot/", user, "/pkg");
					Directory pkg_dir(_root, path);
					_scan_depot_user_pkg(user, pkg_dir, xml);
				});
			});
		}

		if (_blueprint_reporter.enabled()) {
			Reporter::Xml_generator xml(_blueprint_reporter, [&] () {
				query.for_each_sub_node("blueprint", [&] (Xml_node node) {
					Archive::Path pkg = node.attribute_value("pkg", Archive::Path());
					try { _query_blueprint(pkg, xml); }
					catch (...) {
						warning("could not obtain blueprint for '", pkg, "'");
					}
				});
			});
		}

		if (_dependencies_reporter.enabled()) {
			Reporter::Xml_generator xml(_dependencies_reporter, [&] () {
				Dependencies dependencies(_heap, _depot_dir);
				query.for_each_sub_node("dependencies", [&] (Xml_node node) {

					Archive::Path const path = node.attribute_value("path", Archive::Path());

					if (node.attribute_value("source", false))
						_collect_source_dependencies(path, dependencies, Recursion_limit{8});

					if (node.attribute_value("binary", false))
						_collect_binary_dependencies(path, dependencies, Recursion_limit{8});
				});
				dependencies.xml(xml);
			});
		}

		if (_user_reporter.enabled()) {
			Reporter::Xml_generator xml(_user_reporter, [&] () {
				query.for_each_sub_node("user", [&] (Xml_node node) {
					_query_user(node.attribute_value("name", Archive::User()), xml); });
			});
		}
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Depot_query::Main::_scan_depot_user_pkg(Archive::User const &user,
                                             Directory const &dir, Xml_generator &xml)
{
	dir.for_each_entry([&] (Directory::Entry const &entry) {

		if (!dir.file_exists(Directory::Path(entry.name(), "/runtime")))
			return;

		Archive::Path const path(user, "/pkg/", entry.name());

		xml.node("pkg", [&] () { xml.attribute("path", path); });
	});
}


Depot_query::Archive::Path
Depot_query::Main::_find_rom_in_pkg(Directory::Path const &pkg_path,
                                    Rom_label       const &rom_label,
                                    Recursion_limit        recursion_limit)
{
	/*
	 * \throw Directory::Nonexistent_directory
	 */
	Directory pkg_dir(_depot_dir, pkg_path);

	/*
	 * \throw Directory::Nonexistent_file
	 * \throw File::Truncated_during_read
	 */
	File_content archives(_heap, pkg_dir, "archives", File_content::Limit{16*1024});

	Archive::Path result;

	archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {

		/*
		 * \throw Archive::Unknown_archive_type
		 */
		switch (Archive::type(archive_path)) {
		case Archive::SRC:
			{
				Archive::Path const
					rom_path(Archive::user(archive_path),    "/bin/",
					         _architecture,                  "/",
					         Archive::name(archive_path),    "/",
					         Archive::version(archive_path), "/", rom_label);

				if (_depot_dir.file_exists(rom_path))
					result = rom_path;
			}
			break;

		case Archive::RAW:
			{
				Archive::Path const
					rom_path(Archive::user(archive_path),    "/raw/",
					         Archive::name(archive_path),    "/",
					         Archive::version(archive_path), "/", rom_label);

				if (_depot_dir.file_exists(rom_path))
					result = rom_path;
			}
			break;

		case Archive::PKG:
			Archive::Path const result_from_pkg =
				_find_rom_in_pkg(archive_path, rom_label, recursion_limit);
			if (result_from_pkg.valid())
				result = result_from_pkg;
			break;
		}
	});
	return result;
}


void Depot_query::Main::_query_blueprint(Directory::Path const &pkg_path, Xml_generator &xml)
{
	Directory pkg_dir(_root, Directory::Path("depot/", pkg_path));

	File_content runtime(_heap, pkg_dir, "runtime", File_content::Limit{16*1024});

	runtime.xml([&] (Xml_node node) {

		xml.node("pkg", [&] () {

			xml.attribute("name", Archive::name(pkg_path));
			xml.attribute("path", pkg_path);

			Rom_label const config = node.attribute_value("config", Rom_label());
			if (config.valid())
				xml.attribute("config", config);

			Xml_node env_xml = _config.xml().has_sub_node("env")
			                 ? _config.xml().sub_node("env") : "<env/>";

			node.for_each_sub_node([&] (Xml_node node) {

				/* skip non-rom nodes */
				if (!node.has_type("rom"))
					return;

				Rom_label const label = node.attribute_value("label", Rom_label());

				/* skip ROM that is provided by the environment */
				bool provided_by_env = false;
				env_xml.for_each_sub_node("rom", [&] (Xml_node node) {
					if (node.attribute_value("label", Rom_label()) == label)
						provided_by_env = true; });

				if (provided_by_env) {
					xml.node("rom", [&] () {
						xml.attribute("label", label);
						xml.attribute("env", "yes");
					});
					return;
				}

				Archive::Path const rom_path =
					_find_rom_in_pkg(pkg_path, label, Recursion_limit{8});

				if (rom_path.valid()) {
					xml.node("rom", [&] () {
						xml.attribute("label", label);
						xml.attribute("path", rom_path);
					});

				} else {

					xml.node("missing_rom", [&] () {
						xml.attribute("label", label); });
				}
			});

			String<160> comment("\n\n<!-- content of '", pkg_path, "/runtime' -->\n");
			xml.append(comment.string());
			xml.append(node.addr(), node.size());
			xml.append("\n");
		});
	});
}


void Depot_query::Main::_collect_source_dependencies(Archive::Path const &path,
                                                     Dependencies &dependencies,
                                                     Recursion_limit recursion_limit)
{
	try { Archive::type(path); }
	catch (Archive::Unknown_archive_type) {
		warning("archive '", path, "' has unexpected type");
		return;
	}

	dependencies.record(path);

	switch (Archive::type(path)) {

	case Archive::PKG:
		try {
			File_content archives(_heap, Directory(_depot_dir, path),
			                      "archives", File_content::Limit{16*1024});

			archives.for_each_line<Archive::Path>([&] (Archive::Path const &path) {
				_collect_source_dependencies(path, dependencies, recursion_limit); });
		}
		catch (File_content::Nonexistent_file) { }
		break;

	case Archive::SRC:
		try {
			File_content used_apis(_heap, Directory(_depot_dir, path),
			                       "used_apis", File_content::Limit{16*1024});

			typedef String<160> Api;
			used_apis.for_each_line<Archive::Path>([&] (Api const &api) {
				dependencies.record(Archive::Path(Archive::user(path), "/api/", api));
			});
		}
		catch (File_content::Nonexistent_file) { }
		break;

	case Archive::RAW:
		break;
	};
}


void Depot_query::Main::_collect_binary_dependencies(Archive::Path const &path,
                                                     Dependencies &dependencies,
                                                     Recursion_limit recursion_limit)
{
	try { Archive::type(path); }
	catch (Archive::Unknown_archive_type) {
		warning("archive '", path, "' has unexpected type");
		return;
	}

	switch (Archive::type(path)) {

	case Archive::PKG:
		try {
			dependencies.record(path);

			File_content archives(_heap, Directory(_depot_dir, path),
			                      "archives", File_content::Limit{16*1024});

			archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {
				_collect_binary_dependencies(archive_path, dependencies, recursion_limit); });

		} catch (File_content::Nonexistent_file) { }
		break;

	case Archive::SRC:
		dependencies.record(Archive::Path(Archive::user(path), "/bin/",
		                                  _architecture,       "/",
		                                  Archive::name(path), "/",
		                                  Archive::version(path)));
		break;

	case Archive::RAW:
		dependencies.record(path);
		break;
	};
}


void Depot_query::Main::_query_user(Archive::User const &user, Xml_generator &xml)
{
	Directory user_dir(_root, Directory::Path("depot/", user));

	xml.attribute("name", user);

	File_content download(_heap, user_dir, "download", File_content::Limit{4*1024});
	typedef String<256> Url;
	download.for_each_line<Url>([&] (Url const &url) {
		xml.node("url", [&] () { xml.append_sanitized(url.string()); }); });

	File_content pubkey(_heap, user_dir, "pubkey", File_content::Limit{8*1024});
	xml.node("pubkey", [&] () {
		typedef String<80> Line;
		pubkey.for_each_line<Line>([&] (Line const &line) {
			xml.append_sanitized(line.string());
			xml.append("\n");
		});
	});
}


void Component::construct(Genode::Env &env) { static Depot_query::Main main(env); }

