/*
 * \brief  Configuration for config loading and depot initialization
 * \author Norman Feske
 * \date   2018-05-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

namespace Sculpt {

	static void gen_prepare_vfs_start   (Generator &);
	static void gen_prepare_fs_rom_start(Generator &);
	static void gen_prepare_bash_start  (Generator &);
}


void Sculpt::gen_prepare_vfs_start(Generator &g)
{
	gen_common_start_content(g, "vfs",
	                         Cap_quota{200}, Ram_quota{5*1024*1024},
	                         Priority::STORAGE);

	char const * const script =
		"export VERSION=`cat /VERSION`\n"
		"cp -r /rw/config/$VERSION/*  /config/\n"
		"mkdir -p /rw/depot\n"
		"cp -r /config/depot/* /rw/depot\n"
		"exit\n";

	gen_provides<::File_system::Session>(g);

	g.node("config", [&] {

		g.node("vfs", [&] {
			gen_named_node(g, "tar", "bash-minimal.tar");
			gen_named_node(g, "tar", "coreutils-minimal.tar");

			gen_named_node(g, "inline", ".bash_profile", [&] {
				g.append_quoted(script); });

			gen_named_node(g, "dir", "dev", [&] {
				g.node("null",  [&] {});
				g.node("log",   [&] {});
				g.node("zero",  [&] {});
				gen_named_node(g, "inline", "rtc", [&] {
					g.append_quoted("2018-01-01 00:01");
				});
				gen_named_node(g, "dir", "pipe", [&] {
					g.node("pipe", [&] { });
				});
			});

			gen_named_node(g, "dir", "rw", [&] {
				g.node("fs", [&] { g.attribute("label", "target -> /"); }); });

			gen_named_node(g, "dir", "config", [&] {
				g.node("fs", [&] { g.attribute("label", "config -> /"); }); });

			gen_named_node(g, "rom", "VERSION");
		});

		g.node("default-policy", [&] {
			g.attribute("root",      "/");
			g.attribute("writeable", "yes");
		});
	});

	g.tabular_node("route", [&] {
		g.node("any-service", [&] {
			g.node("parent", [&] { }); }); });
}


void Sculpt::gen_prepare_fs_rom_start(Generator &g)
{
	gen_common_start_content(g, "vfs_rom",
	                         Cap_quota{100}, Ram_quota{15*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "cached_fs_rom", [&] { });

	gen_provides<Rom_session>(g);

	g.node("config", [&] { });

	g.node("route", [&] {
		gen_service_node<::File_system::Session>(g, [&] {
			gen_named_node(g, "child", "vfs"); });

		g.node("any-service", [&] { g.node("parent", [&] { }); });
	});
}


void Sculpt::gen_prepare_bash_start(Generator &g)
{
	gen_common_start_content(g, "bash",
	                         Cap_quota{400}, Ram_quota{15*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(g, "binary", "/bin/bash", [&] { });

	g.node("exit", [&] { g.attribute("propagate", "yes"); });

	g.node("config", [&] {

		g.node("libc", [&] {
			g.attribute("stdout", "/dev/null");
			g.attribute("stderr", "/dev/null");
			g.attribute("stdin",  "/dev/null");
			g.attribute("pipe",   "/dev/pipe");
			g.attribute("rtc",    "/dev/rtc");
		});

		g.node("vfs", [&] { g.node("fs", [&] { }); });

		auto gen_env = [&] (auto key, auto value) {
			g.node("env", [&] {
				g.attribute("key",   key);
				g.attribute("value", value); }); };

		gen_env("HOME", "/");
		gen_env("TERM", "screen");
		gen_env("PATH", "/bin");

		g.node("arg", [&] { g.attribute("value", "bash"); });
		g.node("arg", [&] { g.attribute("value", "--login"); });
	});

	g.node("route", [&] {
		gen_service_node<::File_system::Session>(g, [&] {
			gen_named_node(g, "child", "vfs"); });

		gen_service_node<Rom_session>(g, [&] {
			g.attribute("label_last", "/bin/bash");
			gen_named_node(g, "child", "vfs_rom");
		});

		gen_service_node<Rom_session>(g, [&] {
			g.attribute("label_prefix", "/bin");
			gen_named_node(g, "child", "vfs_rom");
		});

		g.node("any-service", [&] { g.node("parent", [&] { }); });
	});
}


void Sculpt::gen_prepare_child_content(Generator &g, Prepare_version version)
{
	gen_child_attr(g, Child_name { "prepare" }, Binary_name { "init" },
	               Cap_quota{800}, Ram_quota{100*1024*1024}, Priority::STORAGE);

	g.attribute("version", version.value);

	g.node("config", [&] {

		g.attribute("prio_levels", 4);

		g.node("parent-provides", [&] {
			gen_parent_service<Rom_session>(g);
			gen_parent_service<Cpu_session>(g);
			gen_parent_service<Pd_session>(g);
			gen_parent_service<Log_session>(g);
			gen_parent_service<Timer::Session>(g);
			gen_parent_service<::File_system::Session>(g);
		});

		g.node("start", [&] { gen_prepare_vfs_start   (g); });
		g.node("start", [&] { gen_prepare_fs_rom_start(g); });
		g.node("start", [&] { gen_prepare_bash_start  (g); });
	});

	g.node("connect", [&] {

		gen_named_node(g, "fs", "vfs -> target", [&] {
			gen_named_node(g, "child", "default_fs_rw"); });

		gen_named_node(g, "fs", "vfs -> config", [&] {
			g.node("parent", [&] { g.attribute("identity", "config"); }); });

		connect_parent_rom(g, "vfs");
		connect_parent_rom(g, "cached_fs_rom");
		connect_parent_rom(g, "bash-minimal.tar");
		connect_parent_rom(g, "coreutils-minimal.tar");
		connect_parent_rom(g, "depot_users.tar");
		connect_parent_rom(g, "vfs.lib.so");
		connect_parent_rom(g, "vfs_pipe.lib.so");
		connect_parent_rom(g, "libc.lib.so");
		connect_parent_rom(g, "libm.lib.so");
		connect_parent_rom(g, "posix.lib.so");
		connect_parent_rom(g, "VERSION");
	});
}
