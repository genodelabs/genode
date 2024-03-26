/*
 * \brief  XML configuration for config loading and depot initialization
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

	static void gen_prepare_vfs_start   (Xml_generator &);
	static void gen_prepare_fs_rom_start(Xml_generator &);
	static void gen_prepare_bash_start  (Xml_generator &);
}


void Sculpt::gen_prepare_vfs_start(Xml_generator &xml)
{
	gen_common_start_content(xml, "vfs",
	                         Cap_quota{200}, Ram_quota{5*1024*1024},
	                         Priority::STORAGE);

	char const * const script =
		"export VERSION=`cat /VERSION`\n"
		"cp -r /rw/config/$VERSION/*  /config/\n"
		"mkdir -p /rw/depot\n"
		"cp -r /config/depot/* /rw/depot\n"
		"exit\n";

	gen_provides<::File_system::Session>(xml);

	xml.node("config", [&] {

		xml.node("vfs", [&] {
			gen_named_node(xml, "tar", "bash-minimal.tar");
			gen_named_node(xml, "tar", "coreutils-minimal.tar");

			gen_named_node(xml, "inline", ".bash_profile", [&] {
				xml.append(script); });

			gen_named_node(xml, "dir", "dev", [&] {
				xml.node("null",  [&] {});
				xml.node("log",   [&] {});
				xml.node("zero",  [&] {});
				gen_named_node(xml, "inline", "rtc", [&] {
					xml.append("2018-01-01 00:01");
				});
				gen_named_node(xml, "dir", "pipe", [&] {
					xml.node("pipe", [&] { });
				});
			});

			gen_named_node(xml, "dir", "rw", [&] {
				xml.node("fs", [&] { xml.attribute("label", "target"); }); });

			gen_named_node(xml, "dir", "config", [&] {
				xml.node("fs", [&] { xml.attribute("label", "config"); }); });

			gen_named_node(xml, "rom", "VERSION");
		});

		xml.node("default-policy", [&] {
			xml.attribute("root",      "/");
			xml.attribute("writeable", "yes");
		});
	});

	xml.node("route", [&] {
		xml.node("any-service", [&] {
			xml.node("parent", [&] { }); }); });
}


void Sculpt::gen_prepare_fs_rom_start(Xml_generator &xml)
{
	gen_common_start_content(xml, "vfs_rom",
	                         Cap_quota{100}, Ram_quota{15*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "fs_rom", [&] { });

	gen_provides<Rom_session>(xml);

	xml.node("config", [&] { });

	xml.node("route", [&] {
		gen_service_node<::File_system::Session>(xml, [&] {
			gen_named_node(xml, "child", "vfs"); });

		xml.node("any-service", [&] { xml.node("parent", [&] { }); });
	});
}


void Sculpt::gen_prepare_bash_start(Xml_generator &xml)
{
	gen_common_start_content(xml, "bash",
	                         Cap_quota{400}, Ram_quota{15*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "/bin/bash", [&] { });

	xml.node("exit", [&] { xml.attribute("propagate", "yes"); });

	xml.node("config", [&] {

		xml.node("libc", [&] {
			xml.attribute("stdout", "/dev/null");
			xml.attribute("stderr", "/dev/null");
			xml.attribute("stdin",  "/dev/null");
			xml.attribute("pipe",   "/dev/pipe");
			xml.attribute("rtc",    "/dev/rtc");
		});

		xml.node("vfs", [&] { xml.node("fs", [&] { }); });

		auto gen_env = [&] (auto key, auto value) {
			xml.node("env", [&] {
				xml.attribute("key",   key);
				xml.attribute("value", value); }); };

		gen_env("HOME", "/");
		gen_env("TERM", "screen");
		gen_env("PATH", "/bin");

		xml.node("arg", [&] { xml.attribute("value", "bash"); });
		xml.node("arg", [&] { xml.attribute("value", "--login"); });
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


void Sculpt::gen_prepare_start_content(Xml_generator &xml, Prepare_version version)
{
	xml.attribute("version", version.value);

	gen_common_start_content(xml, "prepare",
	                         Cap_quota{800}, Ram_quota{100*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "init");

	xml.node("config", [&] {

		xml.attribute("prio_levels", 4);

		xml.node("parent-provides", [&] {
			gen_parent_service<Rom_session>(xml);
			gen_parent_service<Cpu_session>(xml);
			gen_parent_service<Pd_session>(xml);
			gen_parent_service<Log_session>(xml);
			gen_parent_service<Timer::Session>(xml);
			gen_parent_service<::File_system::Session>(xml);
		});

		xml.node("start", [&] { gen_prepare_vfs_start   (xml); });
		xml.node("start", [&] { gen_prepare_fs_rom_start(xml); });
		xml.node("start", [&] { gen_prepare_bash_start  (xml); });
	});

	xml.node("route", [&] {

		gen_service_node<::File_system::Session>(xml, [&] {
			xml.attribute("label", "vfs -> target");
			gen_named_node(xml, "child", "default_fs_rw"); });

		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "bash-minimal.tar");
		gen_parent_rom_route(xml, "coreutils-minimal.tar");
		gen_parent_rom_route(xml, "depot_users.tar");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "vfs_pipe.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "posix.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Rom_session>    (xml);
		gen_parent_route<Timer::Session> (xml);

		gen_service_node<::File_system::Session>(xml, [&] {
			xml.attribute("label", "vfs -> config");
			xml.node("parent", [&] { xml.attribute("label", "config"); }); });
	});
}
