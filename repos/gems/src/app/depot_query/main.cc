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
		Archive::type(archive_path).with_result([&] (Archive::Type type) {

			switch (type) {
			case Archive::SRC:
				Archive::bin_path(archive_path, _architecture).with_result(
					[&] (Archive::Path const &bin_path) {
						if (_file_exists(bin_path, rom_label))
							result = Archive::Path { bin_path, "/", rom_label }; },
					[&] (Archive::Unknown) { });
				break;

			case Archive::RAW:
				if (_file_exists(archive_path, rom_label))
					result = Archive::Path(archive_path, "/", rom_label);
				break;

			case Archive::PKG:

				_with_file_content(archive_path, "archives", [&] (File_content const &archives) {

					Archive::Path const result_from_pkg =
						_find_rom_in_pkg(archives, rom_label, recursion_limit);

					if (result_from_pkg.valid())
						result = result_from_pkg;
				});
				break;

			case Archive::API:
			case Archive::BIN:
			case Archive::DBG:
			case Archive::IMAGE:
			case Archive::INDEX:
				break;
			}
		}, [&] (Archive::Unknown) { warning("unknown archive type: ", archive_path); });
	});
	return result;
}


void Depot_query::Main::_gen_rom_path_nodes(Generator           &g,
                                            Node          const &env_node,
                                            Archive::Path const &pkg_path,
                                            Node          const &runtime)
{
	_with_file_content(pkg_path, "archives", [&] (File_content const &archives) {
		runtime.for_each_sub_node("content", [&] (Node const &content) {
			content.for_each_sub_node([&] (Node const &node) {

				/* skip non-rom nodes */
				if (!node.has_type("rom"))
					return;

				Rom_label const label = node.attribute_value("label", Rom_label());
				Rom_label const as    = node.attribute_value("as",    label);

				/* skip ROM that is provided by the environment */
				bool provided_by_env = false;
				env_node.for_each_sub_node("rom", [&] (Node const &node) {
					if (node.attribute_value("label", Rom_label()) == label)
						provided_by_env = true; });

				auto gen_label_attr = [&]
				{
					g.attribute("label", label);
					if (as != label)
						g.attribute("as", as);
				};

				if (provided_by_env) {
					g.node("rom", [&] () {
						gen_label_attr();
						g.attribute("env", "yes");
					});
					return;
				}

				Archive::Path const rom_path =
					_find_rom_in_pkg(archives, label, Recursion_limit{8});

				if (rom_path.valid()) {
					g.node("rom", [&] () {
						gen_label_attr();
						g.attribute("path", rom_path);
					});

				} else {

					g.node("missing_rom", [&] () {
						g.attribute("label", label); });
				}
			});
		});
	});
}


void Depot_query::Main::_gen_inherited_rom_path_nodes(Generator           &g,
                                                      Node          const &env_node,
                                                      Archive::Path const &pkg_path,
                                                      Recursion_limit      recursion_limit)
{
	_with_file_content(pkg_path, "archives", [&] (File_content const &archives) {
		archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {

			/* early return if archive path is not a valid pkg path */
			bool const pkg = Archive::type(archive_path).convert<bool>(
				[&] (Archive::Type type) { return type == Archive::PKG; },
				[&] (Archive::Unknown)   { return false; });
			if (!pkg)
				return;

			_with_file_content(archive_path, "runtime" , [&] (File_content const &runtime) {
				runtime.node([&] (Node const &node) {
					_gen_rom_path_nodes(g, env_node, pkg_path, node); }); });

			_gen_inherited_rom_path_nodes(g, env_node, archive_path, recursion_limit);
		});
	});
}


void Depot_query::Main::_query_blueprint(Directory::Path const &pkg_path, Generator &g)
{
	Directory pkg_dir(_root, Directory::Path("depot/", pkg_path));

	File_content runtime(_heap, pkg_dir, "runtime", File_content::Limit{16*1024});

	runtime.node([&] (Node const &node) {

		g.node("pkg", [&] () {

			Archive::name(pkg_path).with_result(
				[&] (Archive::Name const &name) { g.attribute("name", name); },
				[]  (Archive::Unknown)          { });

			g.attribute("path", pkg_path);

			Rom_label const config = node.attribute_value("config", Rom_label());
			if (config.valid())
				g.attribute("config", config);

			auto with_env_node = [&] (auto const &fn)
			{
				_config.node().with_sub_node("env",
					[&] (Node const &env) { fn(env); },
					[&]                   { fn(Node()); });
			};

			with_env_node([&] (Node const &env_node) {
				_gen_rom_path_nodes(g, env_node, pkg_path, node);

				_gen_inherited_rom_path_nodes(g, env_node, pkg_path, Recursion_limit{8});

				if (!g.append_node(node, Generator::Max_depth { 20 }))
					warning("pkg runtime of '", pkg_path, "' too deeply nested");
			});
		});
	});
}


void Depot_query::Main::_collect_source_dependencies(Archive::Path const &path,
                                                     Dependencies &dependencies,
                                                     Require_verify require_verify,
                                                     Recursion_limit recursion_limit)
{
	Archive::type(path).with_result([&] (Archive::Type const type) {

		dependencies.record(path, require_verify);

		switch (type) {

		case Archive::PKG: {
			_with_file_content(path, "archives", [&] (File_content const &archives) {
				archives.for_each_line<Archive::Path>([&] (Archive::Path const &path) {
					_collect_source_dependencies(path, dependencies, require_verify,
					                             recursion_limit); }); });
			break;
		}

		case Archive::SRC: {
			using Api = String<160>;
			_with_file_content(path, "used_apis", [&] (File_content const &used_apis) {
				used_apis.for_each_line<Archive::Path>([&] (Api const &api) {
					Archive::user(path).with_result([&] (Archive::User const &user) {
						dependencies.record({ user, "/api/", api }, require_verify);
					}, [&] (Archive::Unknown) { }); }); });
			break;
		}

		case Archive::BIN:
		case Archive::DBG:
			Archive::user(path).with_result([&] (Archive::User const &user) {
				Archive::name(path).with_result([&] (Archive::Name const &name) {
					Archive::version(path).with_result([&] (Archive::Version const &version) {
						dependencies.record({ user, "/src/", name, "/", version }, require_verify);
					}, [&] (Archive::Unknown) { });
				}, [&] (Archive::Unknown) { });
			}, [&] (Archive::Unknown) { });
			break;

		case Archive::API:
		case Archive::RAW:
		case Archive::IMAGE:
		case Archive::INDEX:
			break;
		}
	}, [&] (Archive::Unknown) { warning("archive '", path, "' has unexpected type"); });
}


void Depot_query::Main::_collect_binary_dependencies(Archive::Path const &path,
                                                     Dependencies &dependencies,
                                                     Require_verify require_verify,
                                                     Recursion_limit recursion_limit)
{
	Archive::type(path).with_result([&] (Archive::Type const type) {

		switch (type) {

		case Archive::PKG:
			dependencies.record(path, require_verify);

			_with_file_content(path, "archives", [&] (File_content const &archives) {
				archives.for_each_line<Archive::Path>([&] (Archive::Path const &archive_path) {
					_collect_binary_dependencies(archive_path, dependencies,
					                             require_verify, recursion_limit); }); });
			break;

		case Archive::SRC:
			Archive::bin_path(path, _architecture).with_result(
				[&] (Archive::Path const &bin_path) {
					dependencies.record(bin_path, require_verify); },
				[&] (Archive::Unknown) { });
			break;

		case Archive::RAW:
		case Archive::API:
		case Archive::BIN:
		case Archive::DBG:
			dependencies.record(path, require_verify);
			break;

		case Archive::IMAGE:
		case Archive::INDEX:
			break;
		}
	}, [&] (Archive::Unknown) { warning("archive '", path, "' has unexpected type"); });
}


void Depot_query::Main::_scan_user(Archive::User const &user, Generator &g)
{
	g.node("user", [&] () {

		Directory user_dir(_root, Directory::Path("depot/", user));

		g.attribute("name", user);
		g.attribute("known_pubkey", user_dir.file_exists("pubkey") ? "yes" : "no");

		if (user_dir.file_exists("download")) {
			File_content download(_heap, user_dir, "download", File_content::Limit{4*1024});
			download.for_each_line<Url>([&] (Url const &url) {
				g.tabular_node("url", [&] { g.append_quoted(url.string()); }); });
		}
	});
}


void Depot_query::Main::_query_user(Archive::User const &user, Generator &g)
{
	g.attribute("name", user);

	try {
		Directory user_dir(_root, Directory::Path("depot/", user));

		File_content download(_heap, user_dir, "download", File_content::Limit{4*1024});
		using Url = String<256>;
		download.for_each_line<Url>([&] (Url const &url) {
			g.node("url", [&] () { g.append_quoted(url.string()); }); });

		File_content pubkey(_heap, user_dir, "pubkey", File_content::Limit{8*1024});
		g.node("pubkey", [&] () {
			using Line = String<80>;
			pubkey.for_each_line<Line>([&] (Line const &line) {
				g.append_quoted(line.string());
				g.append_quoted("\n");
			});
		});
	}
	catch (Directory::Nonexistent_file) {
		warning("incomplete depot-user info for '", user, "'"); }

	catch (Directory::Nonexistent_directory) {
		warning("missing depot-user info for '", user, "'"); }
}


void Depot_query::Main::_gen_index_node_rec(Generator &g, Node const &node,
                                            unsigned max_depth) const
{
	if (max_depth == 0) {
		warning("index has too many nesting levels");
		return;
	}

	node.for_each_sub_node([&] (Node const &node) {

		/* check if single index entry is compatible with architecture */
		bool const arch_compatible =
			!node.has_attribute("arch") ||
			(node.attribute_value("arch", Architecture()) == _architecture);

		if (!arch_compatible)
			return;

		if (node.has_type("index")) {
			g.node("index", [&] () {
				g.attribute("name", node.attribute_value("name", String<100>()));
				_gen_index_node_rec(g, node, max_depth - 1);
			});
		}

		if (node.has_type("pkg")) {
			g.node("pkg", [&] () {
				g.attribute("path", node.attribute_value("path", Archive::Path()));
				g.attribute("info", node.attribute_value("info", String<200>()));
			});
		}
	});
};


void Depot_query::Main::_gen_index_for_arch(Generator &g,
                                            Node const &node) const
{
	/* check of architecture is supported by the index */
	bool supports_arch = false;
	node.for_each_sub_node("supports", [&] (Node const &supports) {
		if (supports.attribute_value("arch", Architecture()) == _architecture)
			supports_arch = true; });

	if (!supports_arch)
		return;

	_gen_index_node_rec(g, node, 10);
}


void Depot_query::Main::_query_index(Archive::User    const &user,
                                     Archive::Version const &version,
                                     bool             const  content,
                                     Require_verify   const  require_verify,
                                     Generator              &g)
{
	Directory::Path const index_path("depot/", user, "/index/", version);
	if (!_root.file_exists(index_path)) {
		g.node("missing", [&] () {
			g.attribute("user",    user);
			g.attribute("version", version);
			require_verify.gen_attr(g);
		});
		return;
	}

	g.node("index", [&] () {
		g.attribute("user",    user);
		g.attribute("version", version);
		require_verify.gen_attr(g);

		if (content) {
			try {
				File_content const
					file(_heap, _root, index_path, File_content::Limit{16*1024});

				file.node([&] (Node const &node) {
					_gen_index_for_arch(g, node); });

			} catch (Directory::Nonexistent_file) { }
		}
	});
}


void Depot_query::Main::_query_image(Archive::User  const &user,
                                     Archive::Name  const &name,
                                     Require_verify const require_verify,
                                     Generator           &g)
{
	Directory::Path const image_path("depot/", user, "/image/", name);
	char const *node_type = _root.directory_exists(image_path)
	                      ? "image" : "missing";
	g.node(node_type, [&] () {
		g.attribute("user", user);
		g.attribute("name", name);
		require_verify.gen_attr(g);
	});
}


void Component::construct(Genode::Env &env)
{
	static Depot_query::Main main(env);
}
