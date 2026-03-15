/*
 * \brief  Configuration for the depot-query tool
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

void Sculpt::gen_depot_query_child_content(Generator &g)
{
	gen_child_attr(g,
	               Child_name { "depot_query" }, Binary_name { "depot_query" },
	               Cap_quota{200}, Ram_quota{2*1024*1024},
	               Priority::STORAGE);

	g.node("config", [&] {
		g.attribute("query", "rom");
		g.node("vfs", [&] {
			gen_named_node(g, "dir", "depot", [&] {
				g.node("fs", [&] {}); }); }); });

	g.tabular_node("connect", [&] {
		connect_fs(g, "depot");
		connect_parent_rom(g, "vfs.lib.so");
		connect_config_rom(g, "query", "depot_query");
		connect_report(g);
	});
}
