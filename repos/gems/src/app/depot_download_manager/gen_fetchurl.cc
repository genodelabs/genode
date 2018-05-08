/*
 * \brief  XML configuration for fetchurl
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

void Depot_download_manager::gen_fetchurl_start_content(Xml_generator &xml,
                                                        Import const &import,
                                                        Url const &current_user_url,
                                                        Fetchurl_version version)
{
	gen_common_start_content(xml, "fetchurl",
	                         Cap_quota{500}, Ram_quota{8*1024*1024});

	xml.attribute("version", version.value);
	xml.node("config", [&] () {
		xml.node("libc", [&] () {
			xml.attribute("stdout", "/dev/log");
			xml.attribute("stderr", "/dev/log");
			xml.attribute("rtc",    "/dev/rtc");
			xml.attribute("socket", "/socket");
		});
		xml.node("report", [&] () { xml.attribute("progress", "yes"); });
		xml.node("vfs", [&] () {
			xml.node("dir", [&] () {
				xml.attribute("name", "download");
				xml.node("fs", [&] () {
					xml.attribute("label", "download"); });
			});
			xml.node("dir", [&] () {
				xml.attribute("name", "dev");
				xml.node("log",  [&] () { });
				xml.node("null", [&] () { });
				xml.node("inline",  [&] () {
					xml.attribute("name", "rtc");
					String<64> date("2000-01-01 00:00");
					xml.append(date.string());
				});
				xml.node("inline",  [&] () {
					xml.attribute("name", "random");
					String<64> entropy("01234567890123456789");
					xml.append(entropy.string());
				});
			});
			xml.node("fs", [&] () {
				xml.attribute("label", "tcpip"); });
		});

		import.for_each_download([&] (Archive::Path const &path) {
			typedef String<160> Remote;
			typedef String<160> Local;
			typedef String<16>  Ext;

			Ext    const ext    (".tar.xz");
			Remote const remote (current_user_url, "/", path, ext);
			Local  const local  ("/download/", path, ext);

			xml.node("fetch", [&] () {
				xml.attribute("url", remote);
				xml.attribute("path", local);
			});

			xml.node("fetch", [&] () {
				xml.attribute("url",  Remote(remote, ".sig"));
				xml.attribute("path", Local (local,  ".sig"));
			});
		});
	});

	xml.node("route", [&] () {
		xml.node("service", [&] () {
			xml.attribute("name", File_system::Session::service_name());
			xml.attribute("label", "download");
			xml.node("parent", [&] () {
				xml.attribute("label", "public_rw"); });
		});
		xml.node("service", [&] () {
			xml.attribute("name", File_system::Session::service_name());
			xml.attribute("label", "tcpip");
			xml.node("parent", [&] () {
				xml.attribute("label", "tcpip"); });
		});
		gen_parent_unscoped_rom_route(xml, "fetchurl");
		gen_parent_unscoped_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "curl.lib.so");
		gen_parent_rom_route(xml, "libssh.lib.so");
		gen_parent_rom_route(xml, "libssl.lib.so");
		gen_parent_rom_route(xml, "libcrypto.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "zlib.lib.so");
		gen_parent_rom_route(xml, "pthread.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
		gen_parent_route<Nic::Session>   (xml);
		gen_parent_route<Report::Session>(xml);
	});
}
