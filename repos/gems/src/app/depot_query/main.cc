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

namespace Depot_query {
	using namespace Genode;
	struct Archive;
	struct Main;
}


struct Depot_query::Archive
{
	typedef String<100> Path;
	typedef String<64>  User;
	typedef String<80>  Name;

	enum Type { PKG, RAW, SRC };

	struct Unknown_archive_type : Exception { };

	/**
	 * Return Nth path element
	 *
	 * The first path element corresponds to n == 0.
	 */
	template <typename STRING>
	static STRING _path_element(Path const &path, unsigned n)
	{
		char const *s = path.string();

		/* skip 'n' path elements */
		for (; n > 0; n--) {

			/* search '/' */
			while (*s && *s != '/')
				s++;

			if (*s == 0)
				return STRING();

			/* skip '/' */
			s++;
		}

		/* find '/' marking the end of the path element */
		unsigned i = 0;
		while (s[i] != 0 && s[i] != '/')
			i++;

		return STRING(Cstring(s, i));
	}

	/**
	 * Return archive user of depot-local path
	 */
	static User user(Path const &path) { return _path_element<User>(path, 0); }

	/**
	 * Return archive type of depot-local path
	 *
	 * \throw Unknown_archive_type
	 */
	static Type type(Path const &path)
	{
		typedef String<8> Name;
		Name const name = _path_element<Name>(path, 1);

		if (name == "src") return SRC;
		if (name == "pkg") return PKG;
		if (name == "raw") return RAW;

		throw Unknown_archive_type();
	}

	static Name name(Path const &path) { return _path_element<Name>(path, 2); }
};


struct Depot_query::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Root_directory _root { _env, _heap, _config.xml().sub_node("vfs") };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Reporter _directory_reporter { _env, "directory" };
	Reporter _blueprint_reporter { _env, "blueprint" };

	typedef String<64> Rom_label;
	typedef String<16> Architecture;

	Architecture _architecture;

	Archive::Path _find_rom_in_pkg(Directory::Path const &pkg_path,
	                               Rom_label       const &rom_label,
	                               unsigned        const  nesting_level);

	void _scan_depot_user_pkg(Archive::User const &user, Directory &dir, Xml_generator &xml);
	void _query_pkg(Directory::Path const &path, Xml_generator &xml);

	void _handle_config()
	{
		_config.update();

		Xml_node config = _config.xml();

		_directory_reporter.enabled(config.has_sub_node("scan"));
		_blueprint_reporter.enabled(config.has_sub_node("query"));

		_root.apply_config(config.sub_node("vfs"));

		if (!config.has_attribute("arch"))
			warning("config lacks 'arch' attribute");

		_architecture = config.attribute_value("arch", Architecture());

		if (_directory_reporter.enabled()) {

			Reporter::Xml_generator xml(_directory_reporter, [&] () {
				config.for_each_sub_node("scan", [&] (Xml_node node) {
					Archive::User const user = node.attribute_value("user", Archive::User());
					Directory::Path path("depot/", user, "/pkg");
					Directory pkg_dir(_root, path);
					_scan_depot_user_pkg(user, pkg_dir, xml);
				});
			});
		}

		if (_blueprint_reporter.enabled()) {

			Reporter::Xml_generator xml(_blueprint_reporter, [&] () {
				config.for_each_sub_node("query", [&] (Xml_node node) {
					_query_pkg(node.attribute_value("pkg", Directory::Path()), xml); });
			});
		}
	}

	Main(Env &env) : _env(env) { _handle_config(); }
};


void Depot_query::Main::_scan_depot_user_pkg(Archive::User const &user,
                                             Directory &dir, Xml_generator &xml)
{
	dir.for_each_entry([&] (Directory::Entry &entry) {

		if (!dir.file_exists(Directory::Path(entry.name(), "/runtime")))
			return;

		Archive::Path const path(user, "/pkg/", entry.name());

		xml.node("pkg", [&] () { xml.attribute("path", path); });
	});
}


Depot_query::Archive::Path
Depot_query::Main::_find_rom_in_pkg(Directory::Path const &pkg_path,
                                    Rom_label       const &rom_label,
                                    unsigned        const  nesting_level)
{
	if (nesting_level == 0) {
		error("too deeply nested pkg archives");
		return Archive::Path();
	}

	/*
	 * \throw Directory::Nonexistent_directory
	 */
	Directory depot_dir(_root, Directory::Path("depot"));
	Directory pkg_dir(depot_dir, pkg_path);

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
					rom_path(Archive::user(archive_path), "/bin/",
					         _architecture, "/",
					         Archive::name(archive_path), "/", rom_label);

				if (depot_dir.file_exists(rom_path))
					result = rom_path;
			}
			break;

		case Archive::RAW:
			log(" ", archive_path, " (raw-data archive)");
			break;

		case Archive::PKG:
			// XXX call recursively, adjust 'nesting_level'
			log(" ", archive_path, " (pkg archive)");
			break;
		}
	});
	return result;
}


void Depot_query::Main::_query_pkg(Directory::Path const &pkg_path, Xml_generator &xml)
{
	Directory pkg_dir(_root, Directory::Path("depot/", pkg_path));

	File_content runtime(_heap, pkg_dir, "runtime", File_content::Limit{16*1024});

	runtime.xml([&] (Xml_node node) {

		xml.node("pkg", [&] () {

			xml.attribute("name", Archive::name(pkg_path));
			xml.attribute("path", pkg_path);

			Xml_node env_xml = _config.xml().has_sub_node("env")
			                 ? _config.xml().sub_node("env") : "<env/>";

			node.for_each_sub_node([&] (Xml_node node) {

				/* skip non-rom nodes */
				if (!node.has_type("rom") && !node.has_type("binary"))
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

				unsigned const max_nesting_levels = 8;
				Archive::Path const rom_path =
					_find_rom_in_pkg(pkg_path, label, max_nesting_levels);

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


void Component::construct(Genode::Env &env) { static Depot_query::Main main(env); }

