/*
 * \brief  Configuration for inspect view
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

using namespace Sculpt;


static void for_each_inspected_storage_target(Storage_devices const &devices, auto const &fn)
{
	devices.for_each([&] (Storage_device const &device) {
		device.for_each_partition([&] (Partition const &partition) {
			if (partition.file_system.inspected)
				fn(Storage_target { device.driver, device.port, partition.number }); }); });
}


static void gen_terminal_start(Generator &g)
{
	gen_common_start_content(g, "terminal",
	                         Cap_quota{140}, Ram_quota{36*1024*1024},
	                         Priority::NESTED_MAX);

	gen_provides<Terminal::Session>(g);

	g.tabular_node("route", [&] {
		gen_parent_rom_route(g, "config", "terminal.config");

		gen_parent_route<Rom_session>    (g);
		gen_parent_route<Cpu_session>    (g);
		gen_parent_route<Pd_session>     (g);
		gen_parent_route<Log_session>    (g);
		gen_parent_route<Timer::Session> (g);
		gen_parent_route<Report::Session>(g);
		gen_parent_route<Gui::Session>   (g);
	});
}


static void gen_vfs_start(Generator &g,
                          Storage_devices const &devices,
                          Ram_fs_state const &ram_fs_state)
{
	gen_common_start_content(g, "vfs",
	                         Cap_quota{200}, Ram_quota{6*1024*1024},
	                         Priority::NESTED_MAX);

	gen_provides<::File_system::Session>(g);

	g.node("config", [&] {

		g.node("vfs", [&] {
			gen_named_node(g, "tar", "bash-minimal.tar");
			gen_named_node(g, "tar", "coreutils-minimal.tar");
			gen_named_node(g, "tar", "vim-minimal.tar");
			gen_named_node(g, "tar", "tclsh.tar");
			gen_named_node(g, "tar", "hrd.tar");

			gen_named_node(g, "dir", "dev", [&] {
				g.node("null",      [&] {});
				g.node("zero",      [&] {});
				g.node("terminal",  [&] {});
				gen_named_node(g, "inline", "rtc", [&] {
					g.append_quoted("2018-01-01 00:01");
				});
				gen_named_node(g, "dir", "pipe", [&] {
					g.node("pipe", [&] { });
				});
			});

			auto fs_dir = [&] (String<64> const &label) {
				gen_named_node(g, "dir", label, [&] {
					g.node("fs", [&] {
						g.attribute("buffer_size", 272u << 10);
						g.attribute("label", prefixed_label(label, String<8>("/"))); }); }); };

			fs_dir("config");
			fs_dir("report");

			for_each_inspected_storage_target(devices, [&] (Storage_target const &target) {
				fs_dir(target.label()); });

			if (ram_fs_state.inspected)
				fs_dir("ram");

			gen_named_node(g, "dir", "tmp", [&] {
				g.node("ram", [&] { }); });

			gen_named_node(g, "dir", "share", [&] {
				gen_named_node(g, "dir", "vim", [&] {
					g.node("rom", [&] {
						g.attribute("name", "vimrc"); }); }); });

			gen_named_node(g, "rom", "VERSION");
		});

		g.node("default-policy", [&] {
			g.attribute("root",      "/");
			g.attribute("writeable", "yes");
		});
	});

	g.tabular_node("route", [&] {

		gen_service_node<::File_system::Session>(g, [&] {
			g.attribute("label_prefix", "config ->");
			g.node("parent", [&] { g.attribute("identity", "config"); });
		});

		gen_service_node<::File_system::Session>(g, [&] {
			g.attribute("label_prefix", "report ->");
			g.node("parent", [&] { g.attribute("identity", "report"); });
		});

		gen_service_node<Terminal::Session>(g, [&] {
			gen_named_node(g, "child", "terminal"); });

		g.node("any-service", [&] {
			g.node("parent", [&] { }); });
	});
}


static void gen_fs_rom_start(Generator &g)
{
	gen_common_start_content(g, "vfs_rom",
	                         Cap_quota{100}, Ram_quota{15*1024*1024},
	                         Priority::NESTED_MAX);

	gen_named_node(g, "binary", "cached_fs_rom", [&] { });

	gen_provides<Rom_session>(g);

	g.node("config", [&] { });

	g.tabular_node("route", [&] {
		gen_service_node<::File_system::Session>(g, [&] {
			gen_named_node(g, "child", "vfs"); });

		g.node("any-service", [&] { g.node("parent", [&] { }); });
	});
}


static void gen_bash_start(Generator &g)
{
	gen_common_start_content(g, "bash",
	                         Cap_quota{400}, Ram_quota{16*1024*1024},
	                         Priority::NESTED_MAX);

	gen_named_node(g, "binary", "/bin/bash", [&] { });

	g.node("config", [&] {

		g.node("libc", [&] {
			g.attribute("stdout", "/dev/terminal");
			g.attribute("stderr", "/dev/terminal");
			g.attribute("stdin",  "/dev/terminal");
			g.attribute("pipe",   "/dev/pipe");
			g.attribute("rtc",    "/dev/rtc");
		});

		g.node("vfs", [&] {
			g.node("fs", [&] {
				g.attribute("buffer_size", 272u << 10); }); });

		auto gen_env = [&] (auto key, auto value) {
			g.node("env", [&] {
				g.attribute("name",   key);
				g.append_quoted(value); }); };

		gen_env("HOME", "/");
		gen_env("TERM", "screen");
		gen_env("PATH", "/bin");
		gen_env("PS1",  "inspect:$PWD> ");

		g.node("arg", [&] { g.attribute("value", "bash"); });
	});

	g.tabular_node("route", [&] {
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


void Sculpt::gen_inspect_view(Generator             &g,
                              Storage_devices const &devices,
                              Ram_fs_state    const &ram_fs_state,
                              Inspect_view_version   version)
{
	g.node("start", [&] {

		g.attribute("version", version.value);

		gen_common_start_content(g, "inspect",
		                         Cap_quota{1000}, Ram_quota{76*1024*1024},
		                         Priority::LEITZENTRALE);

		gen_named_node(g, "binary", "init", [&] { });

		g.node("config", [&] {

			g.node("parent-provides", [&] {
				gen_parent_service<Rom_session>(g);
				gen_parent_service<Cpu_session>(g);
				gen_parent_service<Pd_session> (g);
				gen_parent_service<Rm_session> (g);
				gen_parent_service<Log_session>(g);
				gen_parent_service<Timer::Session>(g);
				gen_parent_service<Report::Session>(g);
				gen_parent_service<::File_system::Session>(g);
				gen_parent_service<Gui::Session>(g);
			});

			g.node("start", [&] { gen_terminal_start(g); });
			g.node("start", [&] { gen_vfs_start(g, devices, ram_fs_state); });
			g.node("start", [&] { gen_fs_rom_start(g); });
			g.node("start", [&] { gen_bash_start(g); });
		});

		g.tabular_node("route", [&] {

			gen_service_node<::File_system::Session>(g, [&] {
				g.attribute("label_prefix", "config ->");
				g.node("parent", [&] { g.attribute("identity", "config"); });
			});

			gen_service_node<::File_system::Session>(g, [&] {
				g.attribute("label_prefix", "report ->");
				g.node("parent", [&] { g.attribute("identity", "report"); });
			});

			gen_parent_rom_route(g, "ld.lib.so");
			gen_parent_rom_route(g, "init");
			gen_parent_rom_route(g, "terminal");
			gen_parent_rom_route(g, "vfs");
			gen_parent_rom_route(g, "cached_fs_rom");
			gen_parent_rom_route(g, "vfs.lib.so");
			gen_parent_rom_route(g, "vfs_pipe.lib.so");
			gen_parent_rom_route(g, "vfs_ttf.lib.so");
			gen_parent_rom_route(g, "libc.lib.so");
			gen_parent_rom_route(g, "libm.lib.so");
			gen_parent_rom_route(g, "bash-minimal.tar");
			gen_parent_rom_route(g, "coreutils-minimal.tar");
			gen_parent_rom_route(g, "vim-minimal.tar");
			gen_parent_rom_route(g, "tclsh.tar");
			gen_parent_rom_route(g, "hrd.tar");
			gen_parent_rom_route(g, "ncurses.lib.so");
			gen_parent_rom_route(g, "posix.lib.so");
			gen_parent_rom_route(g, "vimrc", "config -> vimrc");
			gen_parent_rom_route(g, "VERSION");
			gen_parent_rom_route(g, "Vera.ttf");
			gen_parent_rom_route(g, "VeraMono.ttf");
			gen_parent_route<Cpu_session>    (g);
			gen_parent_route<Pd_session>     (g);
			gen_parent_route<Rm_session>     (g);
			gen_parent_route<Log_session>    (g);
			gen_parent_route<Timer::Session> (g);

			for_each_inspected_storage_target(devices, [&] (Storage_target const &target) {
				gen_service_node<::File_system::Session>(g, [&] {
					g.attribute("label_prefix", Session_label("vfs -> ", target.label(), " ->"));
					gen_named_node(g, "child", target.fs()); }); });

			if (ram_fs_state.inspected)
				gen_service_node<::File_system::Session>(g, [&] {
					g.attribute("label_prefix", "vfs -> ram ->");
					gen_named_node(g, "child", "ram_fs"); });

			gen_service_node<Gui::Session>(g, [&] {
				g.node("parent", [&] {
					g.attribute("label", String<64>("leitzentrale -> inspect")); }); });

			gen_service_node<Rom_session>(g, [&] {
				g.attribute("label", "terminal.config");
				g.node("parent", [&] {
					g.attribute("label", String<64>("config -> managed/fonts")); }); });

			gen_service_node<Rom_session>(g, [&] {
				g.attribute("label", "terminal -> clipboard");
				g.node("parent", [&] {
					g.attribute("label", String<64>("inspect -> clipboard")); }); });

			gen_service_node<Report::Session>(g, [&] {
				g.attribute("label", "terminal -> clipboard");
				g.node("parent", [&] {
					g.attribute("label", String<64>("inspect -> clipboard")); }); });
		});
	});
}
