/*
 * \brief  Configuration for the chroot component
 * \author Norman Feske
 * \date   2018-05-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_chroot_start_content(Generator &g, Start_name const &name,
                                      Path const &path, Writeable writeable)
{
	gen_common_start_content(g, name,
	                         Cap_quota{100}, Ram_quota{2*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "chroot");

	g.node("config", [&] {
		g.node("default-policy", [&] {
			g.attribute("path", path);
			if (writeable == WRITEABLE)
				g.attribute("writeable", "yes");
		});
	});

	gen_provides<::File_system::Session>(g);

	g.node("route", [&] {

	 	gen_service_node<::File_system::Session>(g, [&] {
			gen_named_node(g, "child", "default_fs_rw"); });

		gen_parent_rom_route(g, "chroot");
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_route<Cpu_session>(g);
		gen_parent_route<Pd_session> (g);
		gen_parent_route<Log_session>(g);
	});
}
