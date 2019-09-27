/*
 * \brief  XML configuration for the extract tool
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

void Depot_download_manager::gen_extract_start_content(Xml_generator       &xml,
                                                       Import        const &import,
                                                       Path          const &user_path,
                                                       Archive::User const &user)
{
	gen_common_start_content(xml, "extract",
	                         Cap_quota{200}, Ram_quota{12*1024*1024});

	xml.node("config", [&] () {
		xml.attribute("verbose", "yes");

		xml.node("libc", [&] () {
			xml.attribute("stdout",       "/dev/log");
			xml.attribute("stderr",       "/dev/log");
			xml.attribute("rtc",          "/dev/null");
			xml.attribute("cwd",          user_path);
			xml.attribute("update_mtime", "no");
		});

		xml.node("vfs", [&] () {
			xml.node("dir", [&] () {
				xml.attribute("name", "public");
				xml.node("fs", [&] () { xml.attribute("label", "public"); });
			});
			xml.node("dir", [&] () {
				xml.attribute("name", "depot");
				xml.node("dir", [&] () {
					xml.attribute("name", user);
					xml.node("fs", [&] () {
						xml.attribute("label", user_path); });
				});
			});
			xml.node("dir", [&] () {
				xml.attribute("name", "dev");
				xml.node("log",  [&] () { });
				xml.node("null", [&] () { });
			});
		});

		import.for_each_verified_archive([&] (Archive::Path const &path) {

			typedef String<160> Path;

			xml.node("extract", [&] () {
				xml.attribute("archive", Path("/public/", Archive::download_file_path(path)));
				xml.attribute("to",      Path("/depot/",  without_last_path_element(path)));

				if (Archive::index(path))
					xml.attribute("name", Archive::index_version(path));
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
			xml.attribute("label", user_path);
			xml.node("child", [&] () {
				xml.attribute("name", user_path); });
		});
		gen_parent_unscoped_rom_route(xml, "extract");
		gen_parent_unscoped_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "posix.lib.so");
		gen_parent_rom_route(xml, "libarchive.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "zlib.lib.so");
		gen_parent_rom_route(xml, "liblzma.lib.so");
		gen_parent_route<Cpu_session>(xml);
		gen_parent_route<Pd_session> (xml);
		gen_parent_route<Log_session>(xml);
	});
}
