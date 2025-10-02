/*
 * \brief  Configuration for RAM file system
 * \author Norman Feske
 * \date   2018-05-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_ram_fs_start_content(Generator &g, Ram_fs_state const &state)
{
	state.gen_start_node_content(g);

	g.node("binary", [&] { g.attribute("name", "vfs"); });

	gen_provides<::File_system::Session>(g);

	g.tabular_node("route", [&] {
		gen_parent_rom_route(g, "vfs");
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_route<Cpu_session> (g);
		gen_parent_route<Pd_session>  (g);
		gen_parent_route<Log_session> (g);
		gen_parent_rom_route(g, "config", "config -> ram_fs");
		gen_parent_route<Rom_session> (g);
	});
}
