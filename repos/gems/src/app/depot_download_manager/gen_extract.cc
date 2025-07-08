/*
 * \brief  Configuration for the extract tool
 * \author Norman Feske
 * \date   2017-12-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "xml.h"

void Depot_download_manager::gen_extract_start_content(Generator           &g,
                                                       Import        const &import,
                                                       Path          const &user_path,
                                                       Archive::User const &user)
{
	gen_common_start_content(g, "extract",
	                         Cap_quota{200}, Ram_quota{12*1024*1024});

	g.node("config", [&] () {
		g.attribute("verbose", "yes");

		g.node("libc", [&] () {
			g.attribute("stdout",       "/dev/log");
			g.attribute("stderr",       "/dev/log");
			g.attribute("rtc",          "/dev/null");
			g.attribute("cwd",          user_path);
			g.attribute("update_mtime", "no");
		});

		g.node("vfs", [&] () {
			g.node("dir", [&] () {
				g.attribute("name", "public");
				g.node("fs", [&] () {
					g.attribute("buffer_size", 144u << 10);
					g.attribute("label", "public -> /"); });
			});
			g.node("dir", [&] () {
				g.attribute("name", "depot");
				g.node("dir", [&] () {
					g.attribute("name", user);
					g.node("fs", [&] () {
						g.attribute("buffer_size", 144u << 10);
						g.attribute("label", Path(user_path, " -> /")); });
				});
			});
			g.node("dir", [&] () {
				g.attribute("name", "dev");
				g.node("log",  [&] () { });
				g.node("null", [&] () { });
			});
		});

		import.for_each_verified_or_blessed_archive([&] (Archive::Path const &path) {

			using Path = String<160>;

			g.node("extract", [&] () {
				g.attribute("archive", Path("/public/", Archive::download_file_path(path)));
				g.attribute("to",      Path("/depot/",  without_last_path_element(path)));

				if (Archive::index(path))
					g.attribute("name", Archive::index_version(path));

				if (Archive::image_index(path))
					g.attribute("name", "index");
			});
		});
	});

	g.node("route", [&] () {
		g.node("service", [&] () {
			g.attribute("name", File_system::Session::service_name());
			g.attribute("label_prefix", "public ->");
			g.node("parent", [&] () {
				g.attribute("identity", "public"); });
		});
		g.node("service", [&] () {
			g.attribute("name", File_system::Session::service_name());
			g.attribute("label_prefix", Path(user_path, " ->"));
			g.node("child", [&] () {
				g.attribute("name", user_path); });
		});
		gen_parent_unscoped_rom_route(g, "extract");
		gen_parent_unscoped_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "libc.lib.so");
		gen_parent_rom_route(g, "libm.lib.so");
		gen_parent_rom_route(g, "posix.lib.so");
		gen_parent_rom_route(g, "libarchive.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_rom_route(g, "zlib.lib.so");
		gen_parent_rom_route(g, "liblzma.lib.so");
		gen_parent_route<Cpu_session>(g);
		gen_parent_route<Pd_session> (g);
		gen_parent_route<Log_session>(g);
	});
}
