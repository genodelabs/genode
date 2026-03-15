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

void Sculpt::gen_fs_child_content(Generator            &g,
                                  Storage_target const &target,
                                  File_system::Type     fs_type)
{
	gen_child_attr(g, Child_name { target.fs() }, Binary_name { "vfs" },
	               Cap_quota{400}, Ram_quota{72*1024*1024}, Priority::STORAGE);

	g.node("provides", [&] { g.node("fs"); });

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

	g.tabular_node("connect", [&] {
		target.gen_block_session_connect(g);
		connect_parent_rom(g, "vfs.lib.so");
		connect_parent_rom(g, "vfs_rump.lib.so");
		connect_parent_rom(g, "rump.lib.so");
		connect_parent_rom(g, "rump_fs.lib.so");
		g.node("rm", [&] { g.node("parent"); });
	});
}
