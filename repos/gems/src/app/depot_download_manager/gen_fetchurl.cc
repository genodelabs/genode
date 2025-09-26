/*
 * \brief  Configuration for fetchurl
 * \author Norman Feske
 * \date   2017-12-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "node.h"

void Depot_download_manager::gen_fetchurl_start_content(Generator &g,
                                                        Import const &import,
                                                        Url const &current_user_url,
                                                        Pubkey_known pubkey_known,
                                                        Fetchurl_version version)
{
	g.attribute("version", version.value);

	gen_common_start_content(g, "fetchurl",
	                         Cap_quota{500}, Ram_quota{8*1024*1024});

	g.node("config", [&] {
		g.node("libc", [&] {
			g.attribute("stdout", "/dev/log");
			g.attribute("stderr", "/dev/log");
			g.attribute("rtc",    "/dev/rtc");
			g.attribute("pipe",   "/pipe");
			g.attribute("socket", "/socket");
		});
		g.node("report", [&] {
			g.attribute("progress", "yes");
			g.attribute("delay_ms", 250);
		});
		g.node("vfs", [&] {
			g.node("dir", [&] {
				g.attribute("name", "download");
				g.node("fs", [&] {
					g.attribute("buffer_size", 144u << 10);
					g.attribute("label", "download -> /"); });
			});
			g.node("dir", [&] {
				g.attribute("name", "dev");
				g.node("log",  [&] { });
				g.node("null", [&] { });
				g.node("inline", [&] {
					g.attribute("name", "rtc");
					String<64> date("2000-01-01 00:00");
					g.append_quoted(date.string());
				});
				g.node("inline", [&] {
					g.attribute("name", "random");
					String<64> entropy("01234567890123456789"
					                   "01234567890123456789");
					g.append_quoted(entropy.string());
				});
			});
			g.node("dir", [&] {
				g.attribute("name", "pipe");
				g.node("pipe", [&] { });
			});
			g.node("fs", [&] {
				g.attribute("label", "tcpip -> /"); });
		});

		import.for_each_download([&] (Archive::Path const &path) {
			using Remote    = String<160>;
			using Local     = String<160>;
			using File_path = String<100>;

			File_path const file_path = Archive::download_file_path(path);
			Remote    const remote (current_user_url, "/", file_path);
			Local     const local  ("/download/", file_path);

			g.node("fetch", [&] {
				g.attribute("url", remote);
				g.attribute("path", local);
			});

			if (pubkey_known.value) {
				g.node("fetch", [&] {
					g.attribute("url",  Remote(remote, ".sig"));
					g.attribute("path", Local (local,  ".sig"));
				});
			}
		});
	});

	g.node("route", [&] {
		g.node("service", [&] {
			g.attribute("name", File_system::Session::service_name());
			g.attribute("label_prefix", "download ->");
			g.node("parent", [&] {
				g.attribute("identity", "public_rw"); });
		});
		g.node("service", [&] {
			g.attribute("name", File_system::Session::service_name());
			g.attribute("label_prefix", "tcpip ->");
			g.node("parent", [&] {
				g.attribute("identity", "tcpip"); });
		});
		gen_parent_unscoped_rom_route(g, "fetchurl");
		gen_parent_unscoped_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "libc.lib.so");
		gen_parent_rom_route(g, "libm.lib.so");
		gen_parent_rom_route(g, "curl.lib.so");
		gen_parent_rom_route(g, "libssh.lib.so");
		gen_parent_rom_route(g, "libssl.lib.so");
		gen_parent_rom_route(g, "libcrypto.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_rom_route(g, "vfs_pipe.lib.so");
		gen_parent_rom_route(g, "zlib.lib.so");
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Log_session>    (g);
		gen_parent_route<Timer::Session> (g);
		gen_parent_route<Nic::Session>   (g);
		gen_parent_route<Report::Session>(g);
	});
}
