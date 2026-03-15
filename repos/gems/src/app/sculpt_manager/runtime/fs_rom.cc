/*
 * \brief  Configuration for the fs-rom component
 * \author Norman Feske
 * \date   2018-05-09
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_fs_rom_child_content(Generator &g,
                                      Server_name const &server,
                                      Child_state const &state)
{
	state.gen_child_node_content(g);

	g.node("config", [&] { });

	g.node("provides", [&] { g.node("rom"); });

	g.tabular_node("connect", [&] {
		connect_fs(g, server);
		g.node("rm", [&] { g.node("parent"); });
	});
}
