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

void Sculpt::gen_ram_fs_child_content(Generator &g, Ram_fs_state const &state)
{
	state.gen_child_node_content(g);

	g.node("provides", [&] { g.node("fs"); });

	g.tabular_node("connect", [&] {
		connect_parent_rom(g, "vfs.lib.so");
		connect_config_rom(g, "config", "ram_fs");
		g.node("rom", [&] { g.node("parent"); });
	});
}
