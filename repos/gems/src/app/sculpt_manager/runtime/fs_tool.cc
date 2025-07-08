/*
 * \brief  Configuration for fs_tool
 * \author Norman Feske
 * \date   2018-05-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <model/file_operation_queue.h>
#include <runtime.h>

void Sculpt::gen_fs_tool_start_content(Generator &g, Fs_tool_version version,
                                       File_operation_queue const &operations)
{
	g.attribute("version", version.value);

	gen_common_start_content(g, "fs_tool", Cap_quota{200}, Ram_quota{5*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "fs_tool");

	g.node("config", [&] {

		g.attribute("exit",    "yes");
		g.attribute("verbose", "yes");

		g.node("vfs", [&] {

			auto gen_fs = [&] (auto name, auto label, auto buffer_size)
			{
				gen_named_node(g, "dir", name, [&] {
					g.node("fs",  [&] {
						g.attribute("label",       label);
						g.attribute("buffer_size", buffer_size); }); });
			};

			gen_fs("rw",     "target -> /", "1M");
			gen_fs("config", "config -> /", "128K");
		});

		operations.gen_fs_tool_config(g);
	});

	g.node("route", [&] {

		gen_service_node<::File_system::Session>(g, [&] {
			g.attribute("label_prefix", "target ->");
			gen_named_node(g, "child", "default_fs_rw"); });

		gen_parent_rom_route(g, "fs_tool");
		gen_parent_rom_route(g, "ld.lib.so");
		gen_parent_rom_route(g, "vfs.lib.so");
		gen_parent_route<Cpu_session> (g);
		gen_parent_route<Pd_session>  (g);
		gen_parent_route<Log_session> (g);
		gen_parent_route<Rom_session> (g);

		gen_service_node<::File_system::Session>(g, [&] {
			g.attribute("label_prefix", "config ->");
			g.node("parent", [&] { g.attribute("identity", "config"); }); });
	});
}
