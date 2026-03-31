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

void Sculpt::gen_fs_tool_child_content(Generator &g, Fs_tool_version version,
                                       File_operation_queue const &operations)
{
	gen_child_attr(g, Child_name { "fs_tool" }, Binary_name { "fs_tool" },
	               Cap_quota{200}, Ram_quota{5*1024*1024}, Priority::STORAGE);

	g.attribute("version", version.value);

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

			gen_fs("rw",    "target -> /", "1M");
			gen_fs("model", "model -> /", "128K");
		});

		operations.gen_fs_tool_config(g);
	});

	g.tabular_node("connect", [&] {

		gen_named_node(g, "fs", "target", [&] {
			gen_named_node(g, "child", "default_fs_rw"); });

		gen_named_node(g, "fs", "model", [&] {
			gen_named_node(g, "child", "model", [&] {
				g.attribute("identity", "rw"); }); });

		connect_parent_rom(g, "vfs.lib.so");
	});
}
