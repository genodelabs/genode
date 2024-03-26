/*
 * \brief  XML configuration for inspect view
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


static void gen_terminal_start(Xml_generator &xml)
{
	gen_common_start_content(xml, "terminal",
	                         Cap_quota{110}, Ram_quota{18*1024*1024},
	                         Priority::LEITZENTRALE);

	gen_provides<Terminal::Session>(xml);

	xml.node("route", [&] {
		gen_parent_rom_route(xml, "config", "terminal.config");

		gen_parent_route<Rom_session>    (xml);
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
		gen_parent_route<Report::Session>(xml);
		gen_parent_route<Gui::Session> (xml);
	});
}


static void gen_vfs_start(Xml_generator &xml,
                          Storage_devices const &devices,
                          Ram_fs_state const &ram_fs_state)
{
	gen_common_start_content(xml, "vfs",
	                         Cap_quota{200}, Ram_quota{6*1024*1024},
	                         Priority::LEITZENTRALE);

	gen_provides<::File_system::Session>(xml);

	xml.node("config", [&] {

		xml.node("vfs", [&] {
			gen_named_node(xml, "tar", "bash-minimal.tar");
			gen_named_node(xml, "tar", "coreutils-minimal.tar");
			gen_named_node(xml, "tar", "vim-minimal.tar");

			gen_named_node(xml, "dir", "dev", [&] {
				xml.node("null",      [&] {});
				xml.node("zero",      [&] {});
				xml.node("terminal",  [&] {});
				gen_named_node(xml, "inline", "rtc", [&] {
					xml.append("2018-01-01 00:01");
				});
				gen_named_node(xml, "dir", "pipe", [&] {
					xml.node("pipe", [&] { });
				});
			});

			auto fs_dir = [&] (String<64> const &label) {
				gen_named_node(xml, "dir", label, [&] {
					xml.node("fs", [&] {
						xml.attribute("buffer_size", 272u << 10);
						xml.attribute("label", label); }); }); };

			fs_dir("config");
			fs_dir("report");

			for_each_inspected_storage_target(devices, [&] (Storage_target const &target) {
				fs_dir(target.label()); });

			if (ram_fs_state.inspected)
				fs_dir("ram");

			gen_named_node(xml, "dir", "tmp", [&] {
				xml.node("ram", [&] { }); });

			gen_named_node(xml, "dir", "share", [&] {
				gen_named_node(xml, "dir", "vim", [&] {
					xml.node("rom", [&] {
						xml.attribute("name", "vimrc"); }); }); });

			gen_named_node(xml, "rom", "VERSION");
		});

		xml.node("default-policy", [&] {
			xml.attribute("root",      "/");
			xml.attribute("writeable", "yes");
		});
	});

	xml.node("route", [&] {

		gen_service_node<::File_system::Session>(xml, [&] {
			xml.attribute("label", "config");
			xml.node("parent", [&] { xml.attribute("label", "config"); });
		});

		gen_service_node<::File_system::Session>(xml, [&] {
			xml.attribute("label", "report");
			xml.node("parent", [&] { xml.attribute("label", "report"); });
		});

		gen_service_node<Terminal::Session>(xml, [&] {
			gen_named_node(xml, "child", "terminal"); });

		xml.node("any-service", [&] {
			xml.node("parent", [&] { }); });
	});
}


static void gen_fs_rom_start(Xml_generator &xml)
{
	gen_common_start_content(xml, "vfs_rom",
	                         Cap_quota{100}, Ram_quota{15*1024*1024},
	                         Priority::LEITZENTRALE);

	gen_named_node(xml, "binary", "cached_fs_rom", [&] { });

	gen_provides<Rom_session>(xml);

	xml.node("config", [&] { });

	xml.node("route", [&] {
		gen_service_node<::File_system::Session>(xml, [&] {
			gen_named_node(xml, "child", "vfs"); });

		xml.node("any-service", [&] { xml.node("parent", [&] { }); });
	});
}


static void gen_bash_start(Xml_generator &xml)
{
	gen_common_start_content(xml, "bash",
	                         Cap_quota{400}, Ram_quota{16*1024*1024},
	                         Priority::LEITZENTRALE);

	gen_named_node(xml, "binary", "/bin/bash", [&] { });

	xml.node("config", [&] {

		xml.node("libc", [&] {
			xml.attribute("stdout", "/dev/terminal");
			xml.attribute("stderr", "/dev/terminal");
			xml.attribute("stdin",  "/dev/terminal");
			xml.attribute("pipe",   "/dev/pipe");
			xml.attribute("rtc",    "/dev/rtc");
		});

		xml.node("vfs", [&] {
			xml.node("fs", [&] {
				xml.attribute("buffer_size", 272u << 10); }); });

		auto gen_env = [&] (auto key, auto value) {
			xml.node("env", [&] {
				xml.attribute("key",   key);
				xml.attribute("value", value); }); };

		gen_env("HOME", "/");
		gen_env("TERM", "screen");
		gen_env("PATH", "/bin");
		gen_env("PS1",  "inspect:$PWD> ");

		xml.node("arg", [&] { xml.attribute("value", "bash"); });
	});

	xml.node("route", [&] {
		gen_service_node<::File_system::Session>(xml, [&] {
			gen_named_node(xml, "child", "vfs"); });

		gen_service_node<Rom_session>(xml, [&] {
			xml.attribute("label_last", "/bin/bash");
			gen_named_node(xml, "child", "vfs_rom");
		});

		gen_service_node<Rom_session>(xml, [&] {
			xml.attribute("label_prefix", "/bin");
			gen_named_node(xml, "child", "vfs_rom");
		});

		xml.node("any-service", [&] { xml.node("parent", [&] { }); });
	});
}


void Sculpt::gen_inspect_view(Xml_generator         &xml,
                              Storage_devices const &devices,
                              Ram_fs_state    const &ram_fs_state,
                              Inspect_view_version   version)
{
	xml.node("start", [&] {

		xml.attribute("version", version.value);

		gen_common_start_content(xml, "inspect",
		                         Cap_quota{1000}, Ram_quota{76*1024*1024},
		                         Priority::LEITZENTRALE);

		gen_named_node(xml, "binary", "init", [&] { });

		xml.node("config", [&] {

			xml.node("parent-provides", [&] {
				gen_parent_service<Rom_session>(xml);
				gen_parent_service<Cpu_session>(xml);
				gen_parent_service<Pd_session> (xml);
				gen_parent_service<Rm_session> (xml);
				gen_parent_service<Log_session>(xml);
				gen_parent_service<Timer::Session>(xml);
				gen_parent_service<Report::Session>(xml);
				gen_parent_service<::File_system::Session>(xml);
				gen_parent_service<Gui::Session>(xml);
			});

			xml.node("start", [&] { gen_terminal_start(xml); });
			xml.node("start", [&] { gen_vfs_start(xml, devices, ram_fs_state); });
			xml.node("start", [&] { gen_fs_rom_start(xml); });
			xml.node("start", [&] { gen_bash_start(xml); });
		});

		xml.node("route", [&] {

			gen_service_node<::File_system::Session>(xml, [&] {
				xml.attribute("label", "config");
				xml.node("parent", [&] { xml.attribute("label", "config"); });
			});

			gen_service_node<::File_system::Session>(xml, [&] {
				xml.attribute("label", "report");
				xml.node("parent", [&] { xml.attribute("label", "report"); });
			});

			gen_service_node<::File_system::Session>(xml, [&] {
				xml.attribute("label", "report");
				xml.node("parent", [&] { xml.attribute("label", "report"); });
			});

			gen_parent_rom_route(xml, "ld.lib.so");
			gen_parent_rom_route(xml, "init");
			gen_parent_rom_route(xml, "terminal");
			gen_parent_rom_route(xml, "vfs");
			gen_parent_rom_route(xml, "cached_fs_rom");
			gen_parent_rom_route(xml, "vfs.lib.so");
			gen_parent_rom_route(xml, "vfs_pipe.lib.so");
			gen_parent_rom_route(xml, "vfs_ttf.lib.so");
			gen_parent_rom_route(xml, "libc.lib.so");
			gen_parent_rom_route(xml, "libm.lib.so");
			gen_parent_rom_route(xml, "bash-minimal.tar");
			gen_parent_rom_route(xml, "coreutils-minimal.tar");
			gen_parent_rom_route(xml, "vim-minimal.tar");
			gen_parent_rom_route(xml, "ncurses.lib.so");
			gen_parent_rom_route(xml, "posix.lib.so");
			gen_parent_rom_route(xml, "vimrc", "config -> vimrc");
			gen_parent_rom_route(xml, "VERSION");
			gen_parent_rom_route(xml, "Vera.ttf");
			gen_parent_rom_route(xml, "VeraMono.ttf");
			gen_parent_route<Cpu_session>    (xml);
			gen_parent_route<Pd_session>     (xml);
			gen_parent_route<Rm_session>     (xml);
			gen_parent_route<Log_session>    (xml);
			gen_parent_route<Timer::Session> (xml);

			for_each_inspected_storage_target(devices, [&] (Storage_target const &target) {
				gen_service_node<::File_system::Session>(xml, [&] {
					xml.attribute("label_last", target.label());
					gen_named_node(xml, "child", target.fs());
				});
			});

			if (ram_fs_state.inspected)
				gen_service_node<::File_system::Session>(xml, [&] {
					xml.attribute("label_last", "ram");
					gen_named_node(xml, "child", "ram_fs");
				});

			gen_service_node<Gui::Session>(xml, [&] {
				xml.node("parent", [&] {
					xml.attribute("label", String<64>("leitzentrale -> inspect")); }); });

			gen_service_node<Rom_session>(xml, [&] {
				xml.attribute("label", "terminal.config");
				xml.node("parent", [&] {
					xml.attribute("label", String<64>("config -> managed/fonts")); }); });

			gen_service_node<Rom_session>(xml, [&] {
				xml.attribute("label", "terminal -> clipboard");
				xml.node("parent", [&] {
					xml.attribute("label", String<64>("inspect -> clipboard")); }); });

			gen_service_node<Report::Session>(xml, [&] {
				xml.attribute("label", "terminal -> clipboard");
				xml.node("parent", [&] {
					xml.attribute("label", String<64>("inspect -> clipboard")); }); });
		});
	});
}
