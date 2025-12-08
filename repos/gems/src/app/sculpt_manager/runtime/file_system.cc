/*
 * \brief  Configuration for file-system server
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

void Sculpt::gen_fs_start_content(Generator            &g,
                                  Storage_target const &target,
                                  File_system::Type     fs_type)
{
	gen_common_start_content(g, target.fs(),
	                         Cap_quota{400}, Ram_quota{72*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "vfs");

	gen_provides<::File_system::Session>(g);

	g.node("config", [&] {
		g.node("vfs", [&] {
			g.node("rump", [&] {
				switch (fs_type) {
				case File_system::EXT2:  g.attribute("fs", "ext2fs"); break;
				case File_system::FAT16:
				case File_system::FAT32: g.attribute("fs", "msdos");  break;
				case File_system::GEMDOS:
					g.attribute("fs",     "msdos");
					g.attribute("gemdos", "yes");
					break;
				case File_system::UNKNOWN: break;
				};
				g.attribute("ram", "48M");
				g.attribute("writeable", "yes");
			});
		});
		g.node("default-policy", [&] {
			g.attribute("root", "/");
			g.attribute("writeable", "yes");
		});
	});

	g.tabular_node("route", [&] {
		target.gen_block_session_route(g);
		gen_parent_rom_route(g, "vfs");
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_rom_route(g, "vfs_rump.lib.so");
		gen_parent_rom_route(g, "rump.lib.so");
		gen_parent_rom_route(g, "rump_fs.lib.so");
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Rm_session>     (g);
		gen_parent_route<Log_session>    (g);
		gen_parent_route<Timer::Session> (g);
	});
}
