/*
 * \brief  fs-query tool for tracking options and launchers
 * \author Norman Feske
 * \date   2018-08-21
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_launcher_query_child_content(Generator &g)
{
	gen_child_attr(g, Child_name { "launcher_query" }, Binary_name { "fs_query" },
	               Cap_quota{200}, Ram_quota{2*1024*1024}, Priority::STORAGE);

	g.node("config", [&] {
		g.attribute("query", "rom");
		g.node("vfs", [&] {
			g.node("fs", [&] {}); });

		g.node("query", [&] {
			g.attribute("path", "/launcher");
			g.attribute("content", "yes");
		});
	});

	g.tabular_node("connect", [&] {
		connect_parent_rom(g, "vfs.lib.so");
		g.node("fs", [&] {
			g.node("parent", [&] { g.attribute("identity", "config"); }); });
		g.node("report", [&] {
			g.node("parent", [&] { g.attribute("label", "launchers"); }); });
	});
}
