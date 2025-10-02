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

void Sculpt::gen_depot_query_start_content(Generator &g)
{
	gen_common_start_content(g, "depot_query",
	                         Cap_quota{200}, Ram_quota{2*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "depot_query");

	g.node("config", [&] {
		g.attribute("query", "rom");
		g.node("vfs", [&] {
			gen_named_node(g, "dir", "depot", [&] {
				g.node("fs", [&] {}); }); }); });

	g.tabular_node("route", [&] {
		gen_service_node<::File_system::Session>(g, [&] {
			gen_named_node(g, "child", "depot"); });

		gen_parent_rom_route(g, "depot_query");
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_rom_route(g, "query", "config -> managed/depot_query");

		gen_parent_route<Cpu_session>     (g);
		gen_parent_route<Pd_session>      (g);
		gen_parent_route<Log_session>     (g);
		gen_parent_route<Report::Session> (g);
	});
}
