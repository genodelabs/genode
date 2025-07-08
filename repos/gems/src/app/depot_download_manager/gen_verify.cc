/*
 * \brief  Configuration for the verify tool
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

void Depot_download_manager::gen_verify_start_content(Generator &g,
                                                      Import const &import,
                                                      Path const &user_path)
{
	gen_common_start_content(g, "verify",
	                         Cap_quota{200}, Ram_quota{12*1024*1024});

	g.node("config", [&] () {
		g.attribute("verbose", "yes");

		g.node("libc", [&] () {
			g.attribute("stdout", "/dev/null");
			g.attribute("stderr", "/dev/null");
			g.attribute("rtc",    "/dev/null");
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
				g.node("fs", [&] () {
					g.attribute("buffer_size", 144u << 10);
					g.attribute("label", "depot -> /"); });
			});
			g.node("dir", [&] () {
				g.attribute("name", "dev");
				g.node("log",  [&] () { });
				g.node("null", [&] () { });
			});
		});

		import.for_each_unverified_archive([&] (Archive::Path const &path) {

			using Path = String<160>;

			Path const file_path   ("/public/", Archive::download_file_path(path));
			Path const pubkey_path (user_path, "/pubkey");

			g.node("verify", [&] () {
				g.attribute("path",   file_path);
				g.attribute("pubkey", pubkey_path);
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
			g.attribute("label_prefix", "depot ->");
			g.node("parent", [&] () {
				g.attribute("identity", "depot"); });
		});
		gen_parent_unscoped_rom_route(g, "verify");
		gen_parent_unscoped_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "libc.lib.so");
		gen_parent_rom_route(g, "libm.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Log_session>    (g);
		gen_parent_route<Report::Session>(g);
	});
}
