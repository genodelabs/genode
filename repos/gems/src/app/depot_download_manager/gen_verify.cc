/*
 * \brief  XML configuration for the verify tool
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

void Depot_download_manager::gen_verify_start_content(Xml_generator &xml,
                                                      Import const &import,
                                                      Path const &user_path)
{
	gen_common_start_content(xml, "verify",
	                         Cap_quota{200}, Ram_quota{12*1024*1024});

	xml.node("config", [&] () {
		xml.attribute("verbose", "yes");

		xml.node("libc", [&] () {
			xml.attribute("stdout", "/dev/null");
			xml.attribute("stderr", "/dev/null");
			xml.attribute("rtc",    "/dev/null");
		});

		xml.node("vfs", [&] () {
			xml.node("dir", [&] () {
				xml.attribute("name", "public");
				xml.node("fs", [&] () { xml.attribute("label", "public"); });
			});
			xml.node("dir", [&] () {
				xml.attribute("name", "depot");
				xml.node("fs", [&] () { xml.attribute("label", "depot"); });
			});
			xml.node("dir", [&] () {
				xml.attribute("name", "dev");
				xml.node("log",  [&] () { });
				xml.node("null", [&] () { });
			});
		});

		import.for_each_unverified_archive([&] (Archive::Path const &path) {

			typedef String<160> Path;
			typedef String<16>  Ext;

			Ext  const ext         (".tar.xz");
			Path const tar_path    ("/public/", path, ext);
			Path const pubkey_path (user_path, "/pubkey");

			xml.node("verify", [&] () {
				xml.attribute("path",   tar_path);
				xml.attribute("pubkey", pubkey_path);
			});
		});
	});

	xml.node("route", [&] () {
		xml.node("service", [&] () {
			xml.attribute("name", File_system::Session::service_name());
			xml.attribute("label", "public");
			xml.node("parent", [&] () {
				xml.attribute("label", "public"); });
		});
		xml.node("service", [&] () {
			xml.attribute("name", File_system::Session::service_name());
			xml.attribute("label", "depot");
			xml.node("parent", [&] () {
				xml.attribute("label", "depot"); });
		});
		gen_parent_unscoped_rom_route(xml, "verify");
		gen_parent_unscoped_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "pthread.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Report::Session>(xml);
	});
}
