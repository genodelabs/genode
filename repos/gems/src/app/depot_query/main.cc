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

/* local includes */
#include <main.h>


Depot_query::Archive::Path
Depot_query::Main::_find_rom_in_pkg(File_content    const &archives,
                                    Rom_label       const &rom_label,
                                    Recursion_limit        recursion_limit)
{
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
					         _architecture,               "/",
					         Archive::name(archive_path), "/",
					         Archive::version(archive_path));

				if (_file_exists(rom_path, rom_label))
					result = Archive::Path(rom_path, "/", rom_label);
			}
			break;

		case Archive::RAW:
			{
				Archive::Path const
					rom_path(Archive::user(archive_path), "/raw/",
					         Archive::name(archive_path), "/",
					         Archive::version(archive_path));

				if (_file_exists(rom_path, rom_label))
					result = Archive::Path(rom_path, "/", rom_label);
			}
			break;

		case Archive::PKG:

			_with_file_content(archive_path, "archives", [&] (File_content const &archives) {

				Archive::Path const result_from_pkg =
					_find_rom_in_pkg(archives, rom_label, recursion_limit);

				if (result_from_pkg.valid())
					result = result_from_pkg;
			});
			break;

		case Archive::BIN:
		case Archive::DBG:
		case Archive::IMAGE:
			break;
		}
	});
	return result;
}


void Depot_query::Main::_gen_rom_path_nodes(Xml_generator       &xml,
                                            Xml_node      const &env_xml,
                                            Archive::Path const &pkg_path,
                                            Xml_node      const &runtime)
{
	_with_file_content(pkg_path, "archives", [&] (File_content const &archives) {
		runtime.for_each_sub_node("content", [&] (Xml_node content) {
			content.for_each_sub_node([&] (Xml_node node) {

				/* skip non-rom nodes */
				if (!node.has_type("rom"))
					return;

				Rom_label const label = node.attribute_value("label", Rom_label());
				Rom_label const as    = node.attribute_value("as",    label);

				/* skip ROM that is provided by the environment */
				bool provided_by_env = false;
				env_xml.for_each_sub_node("rom", [&] (Xml_node node) {
					if (node.attribute_value("label", Rom_label()) == label)
						provided_by_env = true; });

				auto gen_label_attr = [&]
				{
					xml.attribute("label", label);
					if (as != label)
						xml.attribute("as", as);
				};

				if (provided_by_env) {
					xml.node("rom", [&] () {
						gen_label_attr();
						xml.attribute("env", "yes");
					});
					return;
				}

				Archive::Path const rom_path =
					_find_rom_in_pkg(archives, label, Recursion_limit{8});

				if (rom_path.valid()) {
					xml.node("rom", [&] () {
						gen_label_attr();
						xml.attribute("path", rom_path);
					});

				} else {

					xml.node("missing_rom", [&] () {
						xml.attribute("label", label); });
				}
			});
		});
	});
}


void Depot_query::Main::_gen_inherited_rom_path_nodes(Xml_generator       &xml,
                                                      Xml_node      const &env_xml,
                                                      Archive::Path const &pkg_path,
                                                      Recursion_limit      recursion_limit)
{
	_with_file_content(pkg_path, "archives", [&] (File_content const &archives) {
		archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {

			/* early return if archive path is not a valid pkg path */
			try {
				if (Archive::type(archive_path) != Archive::PKG)
					return;
			}
			catch (Archive::Unknown_archive_type) { return; }

			_with_file_content(archive_path, "runtime" , [&] (File_content const &runtime) {
				runtime.xml([&] (Xml_node node) {
					_gen_rom_path_nodes(xml, env_xml, pkg_path, node); }); });

			_gen_inherited_rom_path_nodes(xml, env_xml, archive_path, recursion_limit);
		});
	});
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

			Xml_node const env_xml = _config.xml().has_sub_node("env")
			                       ? _config.xml().sub_node("env") : "<env/>";

			_gen_rom_path_nodes(xml, env_xml, pkg_path, node);

			_gen_inherited_rom_path_nodes(xml, env_xml, pkg_path, Recursion_limit{8});

			String<160> comment("\n\n<!-- content of '", pkg_path, "/runtime' -->\n");
			xml.append(comment.string());
			node.with_raw_node([&] (char const *start, size_t length) {
				xml.append(start, length); });
			xml.append("\n");
		});
	});
}


void Depot_query::Main::_collect_source_dependencies(Archive::Path const &path,
                                                     Dependencies &dependencies,
                                                     Require_verify require_verify,
                                                     Recursion_limit recursion_limit)
{
	try { Archive::type(path); }
	catch (Archive::Unknown_archive_type) {
		warning("archive '", path, "' has unexpected type");
		return;
	}

	dependencies.record(path, require_verify);

	switch (Archive::type(path)) {

	case Archive::PKG: {
		_with_file_content(path, "archives", [&] (File_content const &archives) {
			archives.for_each_line<Archive::Path>([&] (Archive::Path const &path) {
				_collect_source_dependencies(path, dependencies, require_verify,
				                             recursion_limit); }); });
		break;
	}

	case Archive::SRC: {
		typedef String<160> Api;
		_with_file_content(path, "used_apis", [&] (File_content const &used_apis) {
			used_apis.for_each_line<Archive::Path>([&] (Api const &api) {
				dependencies.record(Archive::Path(Archive::user(path), "/api/", api),
				                    require_verify); }); });
		break;
	}

	case Archive::BIN:
	case Archive::DBG:
		dependencies.record(Archive::Path(Archive::user(path), "/src/",
		                                  Archive::name(path), "/",
		                                  Archive::version(path)), require_verify);
		break;
	case Archive::RAW:
	case Archive::IMAGE:
		break;
	};
}


void Depot_query::Main::_collect_binary_dependencies(Archive::Path const &path,
                                                     Dependencies &dependencies,
                                                     Require_verify require_verify,
                                                     Recursion_limit recursion_limit)
{
	try { Archive::type(path); }
	catch (Archive::Unknown_archive_type) {
		warning("archive '", path, "' has unexpected type");
		return;
	}

	switch (Archive::type(path)) {

	case Archive::PKG:
		dependencies.record(path, require_verify);

		_with_file_content(path, "archives", [&] (File_content const &archives) {
			archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {
				_collect_binary_dependencies(archive_path, dependencies,
				                             require_verify, recursion_limit); }); });
		break;

	case Archive::SRC:
		dependencies.record(Archive::Path(Archive::user(path), "/bin/",
		                                  _architecture,       "/",
		                                  Archive::name(path), "/",
		                                  Archive::version(path)), require_verify);
		break;

	case Archive::RAW:
	case Archive::BIN:
	case Archive::DBG:
		dependencies.record(path, require_verify);
		break;

	case Archive::IMAGE:
		break;
	};
}


void Depot_query::Main::_scan_user(Archive::User const &user, Xml_generator &xml)
{
	xml.node("user", [&] () {

		Directory user_dir(_root, Directory::Path("depot/", user));

		xml.attribute("name", user);
		xml.attribute("known_pubkey", user_dir.file_exists("pubkey") ? "yes" : "no");

		if (user_dir.file_exists("download")) {
			File_content download(_heap, user_dir, "download", File_content::Limit{4*1024});
			download.for_each_line<Url>([&] (Url const &url) {
				xml.node("url", [&] () { xml.append_sanitized(url.string()); }); });
		}
	});
}


void Depot_query::Main::_query_user(Archive::User const &user, Xml_generator &xml)
{
	xml.attribute("name", user);

	try {
		Directory user_dir(_root, Directory::Path("depot/", user));

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
	catch (Directory::Nonexistent_file) {
		warning("incomplete depot-user info for '", user, "'"); }

	catch (Directory::Nonexistent_directory) {
		warning("missing depot-user info for '", user, "'"); }
}


void Depot_query::Main::_gen_index_node_rec(Xml_generator  &xml,
                                            Xml_node const &node,
                                            unsigned max_depth) const
{
	if (max_depth == 0) {
		warning("index has too many nesting levels");
		return;
	}

	node.for_each_sub_node([&] (Xml_node const &node) {

		/* check if single index entry is compatible with architecture */
		bool const arch_compatible =
			!node.has_attribute("arch") ||
			(node.attribute_value("arch", Architecture()) == _architecture);

		if (!arch_compatible)
			return;

		if (node.has_type("index")) {
			xml.node("index", [&] () {
				xml.attribute("name", node.attribute_value("name", String<100>()));
				_gen_index_node_rec(xml, node, max_depth - 1);
			});
		}

		if (node.has_type("pkg")) {
			xml.node("pkg", [&] () {
				xml.attribute("path", node.attribute_value("path", Archive::Path()));
				xml.attribute("info", node.attribute_value("info", String<200>()));
			});
		}
	});
};


void Depot_query::Main::_gen_index_for_arch(Xml_generator &xml,
                                            Xml_node const &node) const
{
	/* check of architecture is supported by the index */
	bool supports_arch = false;
	node.for_each_sub_node("supports", [&] (Xml_node const &supports) {
		if (supports.attribute_value("arch", Architecture()) == _architecture)
			supports_arch = true; });

	if (!supports_arch)
		return;

	_gen_index_node_rec(xml, node, 10);
}


void Depot_query::Main::_query_index(Archive::User    const &user,
                                     Archive::Version const &version,
                                     bool             const  content,
                                     Require_verify   const  require_verify,
                                     Xml_generator          &xml)
{
	Directory::Path const index_path("depot/", user, "/index/", version);
	if (!_root.file_exists(index_path)) {
		xml.node("missing", [&] () {
			xml.attribute("user",    user);
			xml.attribute("version", version);
			require_verify.gen_attr(xml);
		});
		return;
	}

	xml.node("index", [&] () {
		xml.attribute("user",    user);
		xml.attribute("version", version);
		require_verify.gen_attr(xml);

		if (content) {
			try {
				File_content const
					file(_heap, _root, index_path, File_content::Limit{16*1024});

				file.xml([&] (Xml_node node) {
					_gen_index_for_arch(xml, node); });

			} catch (Directory::Nonexistent_file) { }
		}
	});
}


void Depot_query::Main::_query_image(Archive::User  const &user,
                                     Archive::Name  const &name,
                                     Require_verify const require_verify,
                                     Xml_generator       &xml)
{
	Directory::Path const image_path("depot/", user, "/image/", name);
	char const *node_type = _root.directory_exists(image_path)
	                      ? "image" : "missing";
	xml.node(node_type, [&] () {
		xml.attribute("user", user);
		xml.attribute("name", name);
		require_verify.gen_attr(xml);
	});
}


void Component::construct(Genode::Env &env)
{
	static Depot_query::Main main(env);
}
