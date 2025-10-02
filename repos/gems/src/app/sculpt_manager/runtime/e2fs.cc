/*
 * \brief  Configuration for invoking e2fstools
 * \author Norman Feske
 * \date   2018-05-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

namespace Sculpt {

	void gen_e2fs_start_content(Generator &, Storage_target const &,
	                            Rom_name const &, auto const &);

	void gen_arg(Generator &g, auto const &arg)
	{
		g.node("arg", [&] { g.attribute("value", arg); });
	}
}


void Sculpt::gen_e2fs_start_content(Generator            &g,
                                    Storage_target const &target,
                                    Rom_name       const &tool,
                                    auto           const &gen_args_fn)
{
	gen_common_start_content(g, String<64>(target.label(), ".", tool),
	                         Cap_quota{500}, Ram_quota{100*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", tool);

	g.node("config", [&] {
		g.node("libc", [&] {
			g.attribute("stdout", "/dev/log");
			g.attribute("stderr", "/dev/log");
			g.attribute("stdin",  "/dev/null");
			g.attribute("rtc",    "/dev/rtc");
		});
		g.node("vfs", [&] {
			gen_named_node(g, "dir", "dev", [&] {
				gen_named_node(g, "block", "block", [&] {
					g.attribute("label", "default");
				});
				gen_named_node(g, "inline", "rtc", [&] {
					g.append_quoted("2018-01-01 00:01");
				});
				g.node("null", [&] {});
				g.node("log",  [&] {});
			});
		});
		gen_args_fn(g);
	});

	g.tabular_node("route", [&] {
		target.gen_block_session_route(g);
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Log_session>    (g);
		gen_parent_route<Rom_session>    (g);
		gen_parent_route<Timer::Session> (g);
	});
}


void Sculpt::gen_fsck_ext2_start_content(Generator &g,
                                         Storage_target const &target)
{
	auto gen_args = [&] (Generator &g) {
		gen_arg(g, "fsck.ext2");
		gen_arg(g, "-yv");
		gen_arg(g, "/dev/block");
	};

	gen_e2fs_start_content(g, target, "e2fsck", gen_args);
}


void Sculpt::gen_mkfs_ext2_start_content(Generator &g,
                                         Storage_target const &target)
{
	auto gen_args = [&] (Generator &g) {
		gen_arg(g, "mkfs.ext2");
		gen_arg(g, "-F");
		gen_arg(g, "/dev/block");
	};

	gen_e2fs_start_content(g, target, "mke2fs", gen_args);
}


void Sculpt::gen_resize2fs_start_content(Generator &g,
                                         Storage_target const &target)
{
	auto gen_args = [&] (Generator &g) {
		gen_arg(g, "resize2fs");
		gen_arg(g, "-f");
		gen_arg(g, "-p");
		gen_arg(g, "/dev/block");
	};

	gen_e2fs_start_content(g, target, "resize2fs", gen_args);
}
