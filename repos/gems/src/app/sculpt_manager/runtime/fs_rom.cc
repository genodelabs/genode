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

void Sculpt::gen_fs_rom_start_content(Generator &g,
                                      Start_name const &binary,
                                      Start_name const &server,
                                      Child_state const &state)
{
	state.gen_start_node_content(g);

	gen_named_node(g, "binary", binary);

	g.node("config", [&] { });

	gen_provides<Rom_session>(g);

	g.tabular_node("route", [&] {

		gen_service_node<::File_system::Session>(g, [&] {
			gen_named_node(g, "child", server); });

		gen_parent_rom_route(g, binary);
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_route<Cpu_session>(g);
		gen_parent_route<Pd_session> (g);
		gen_parent_route<Log_session>(g);
		gen_parent_route<Rm_session> (g);
	});
}
