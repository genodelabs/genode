/*
 * \brief  XML configuration for file-system server
 * \author Norman Feske
 * \date   2018-05-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_fs_start_content(Xml_generator        &xml,
                                  Storage_target const &target,
                                  File_system::Type     fs_type)
{
	gen_common_start_content(xml, target.fs(),
	                         Cap_quota{400}, Ram_quota{72*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "vfs");

	gen_provides<::File_system::Session>(xml);

	xml.node("config", [&] {
		xml.node("vfs", [&] {
			xml.node("rump", [&] {
				switch (fs_type) {
				case File_system::EXT2:  xml.attribute("fs", "ext2fs"); break;
				case File_system::FAT32: xml.attribute("fs", "msdos");  break;
				case File_system::GEMDOS:
					xml.attribute("fs",     "msdos");
					xml.attribute("gemdos", "yes");
					break;
				case File_system::UNKNOWN: break;
				};
				xml.attribute("ram", "48M");
				xml.attribute("writeable", "yes");
			});
		});
		xml.node("default-policy", [&] {
			xml.attribute("root", "/");
			xml.attribute("writeable", "yes");
		});
	});

	xml.node("route", [&] {
		target.gen_block_session_route(xml);
		gen_parent_rom_route(xml, "vfs");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "vfs_rump.lib.so");
		gen_parent_rom_route(xml, "rump.lib.so");
		gen_parent_rom_route(xml, "rump_fs.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Rm_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
	});
}
