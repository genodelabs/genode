/*
 * \brief  XML configuration for the fs-query tool for obtaining the launchers
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

void Sculpt::gen_launcher_query_start_content(Generator &g)
{
	gen_common_start_content(g, "launcher_query",
	                         Cap_quota{200}, Ram_quota{2*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "fs_query");

	g.node("config", [&] {
		g.attribute("query", "rom");
		g.node("vfs", [&] {
			g.node("fs", [&] {}); });

		g.node("query", [&] {
			g.attribute("path", "/launcher");
			g.attribute("content", "yes");
		});

		g.node("query", [&] {
			g.attribute("path", "/presets");
			g.attribute("content", "yes");
		});
	});

	g.node("route", [&] {
		gen_parent_rom_route(g, "fs_query");
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");

		gen_parent_route<Cpu_session>     (g);
		gen_parent_route<Pd_session>      (g);
		gen_parent_route<Log_session>     (g);
		gen_parent_route<Report::Session> (g);

		gen_service_node<::File_system::Session>(g, [&] {
			g.node("parent", [&] {
				g.attribute("identity", "config"); }); });
	});
}
