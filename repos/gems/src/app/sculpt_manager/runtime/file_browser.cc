/*
 * \brief  XML configuration for file-browser subsystem
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

	template <typename FN>
	void for_each_inspected_storage_target(Storage_devices const &devices, FN const &fn)
	{
		devices.for_each([&] (Storage_device const &device) {
			device.for_each_partition([&] (Partition const &partition) {
				if (partition.file_system_inspected)
					fn(Storage_target { device.label, partition.number }); }); });
	}

	void gen_nit_fb_start(Xml_generator &, Rom_name const &);
	void gen_terminal_start(Xml_generator &, Rom_name const &, Rom_name const &,
	                        File_browser_version);
	void gen_noux_start(Xml_generator &, Rom_name const &, Rom_name const &,
	                    Storage_devices const &, Ram_fs_state const &, File_browser_version);
}


void Sculpt::gen_nit_fb_start(Xml_generator &xml, Rom_name const &name)
{
	gen_common_start_content(xml, name, Cap_quota{100}, Ram_quota{18*1024*1024});

	gen_named_node(xml, "binary", "nit_fb");

	xml.node("provides", [&] () {
		gen_service_node<Framebuffer::Session>(xml, [&] () {});
		gen_service_node<Input::Session>(xml, [&] () {});
	});

	xml.node("config", [&] () { });

	xml.node("route", [&] () {

		gen_service_node<Nitpicker::Session>(xml, [&] () {
			xml.node("parent", [&] () {
				xml.attribute("label", String<64>("leitzentrale -> ", name)); }); });

		gen_parent_rom_route(xml, "nit_fb");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session> (xml);
		gen_parent_route<Pd_session>  (xml);
		gen_parent_route<Log_session> (xml);
	});
}


void Sculpt::gen_terminal_start(Xml_generator &xml, Rom_name const &name,
                                Rom_name const &nit_fb_name,
                                File_browser_version version)
{
	xml.attribute("version", version.value);

	gen_common_start_content(xml, name, Cap_quota{100}, Ram_quota{4*1024*1024});

	gen_named_node(xml, "binary", "terminal");

	gen_provides<Terminal::Session>(xml);

	xml.node("route", [&] () {
		gen_service_node<Framebuffer::Session>(xml, [&] () {
			gen_named_node(xml, "child", nit_fb_name); });

		gen_service_node<Input::Session>(xml, [&] () {
			gen_named_node(xml, "child", nit_fb_name);  });

		gen_parent_rom_route(xml, "terminal");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "vfs_ttf.lib.so");
		gen_parent_rom_route(xml, "Vera.ttf");
		gen_parent_rom_route(xml, "VeraMono.ttf");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
		gen_parent_route<Report::Session>(xml);

		gen_named_node(xml, "service", Rom_session::service_name(), [&] () {
			xml.attribute("label", "clipboard");
			xml.node("parent", [&] () { }); });

		gen_named_node(xml, "service", Rom_session::service_name(), [&] () {
			xml.attribute("label", "config");
			xml.node("parent", [&] () {
				xml.attribute("label", "config -> managed/fonts"); }); });
	});
}


void Sculpt::gen_noux_start(Xml_generator &xml, Rom_name const &name,
                            Rom_name const &terminal_name,
                            Storage_devices const &devices,
                            Ram_fs_state const &ram_fs_state,
                            File_browser_version version)
{
	xml.attribute("version", version.value);

	gen_common_start_content(xml, name, Cap_quota{500}, Ram_quota{64*1024*1024});

	gen_named_node(xml, "binary", "noux");

	xml.node("config", [&] () {
		xml.node("fstab", [&] () {
			gen_named_node(xml, "tar", "bash-minimal.tar");
			gen_named_node(xml, "tar", "coreutils-minimal.tar");
			gen_named_node(xml, "tar", "vim-minimal.tar");
			gen_named_node(xml, "dir", "dev", [&] () {
				xml.node("null",  [&] () {});
				xml.node("zero",  [&] () {});
			});
			gen_named_node(xml, "dir", "share", [&] () {
				gen_named_node(xml, "tar", "depot_users.tar"); });

			auto fs_dir = [&] (String<64> const &label) {
				gen_named_node(xml, "dir", label, [&] () {
					xml.node("fs", [&] () { xml.attribute("label", label); }); }); };

			fs_dir("config");
			fs_dir("report");

			for_each_inspected_storage_target(devices, [&] (Storage_target const &target) {
				fs_dir(target.label()); });

			if (ram_fs_state.inspected)
				fs_dir("ram");

			gen_named_node(xml, "dir", "tmp", [&] () {
				xml.node("ram", [&] () { }); });

			gen_named_node(xml, "dir", "share", [&] () {
				gen_named_node(xml, "dir", "vim", [&] () {
					xml.node("rom", [&] () {
						xml.attribute("name", "vimrc"); }); }); });

			gen_named_node(xml, "rom", "VERSION");
		});

		gen_named_node(xml, "start", "/bin/bash", [&] () {

			gen_named_node(xml, "env", "TERM", [&] () {
				xml.attribute("value", "screen"); });

			gen_named_node(xml, "env", "PS1", [&] () {
				xml.attribute("value", "inspect:$PWD> "); });
		});
	});

	xml.node("route", [&] () {

		gen_service_node<::File_system::Session>(xml, [&] () {
			xml.attribute("label", "config");
			xml.node("parent", [&] () { xml.attribute("label", "config"); });
		});

		gen_service_node<Terminal::Session>(xml, [&] () {
			gen_named_node(xml, "child", terminal_name); });

		gen_parent_rom_route(xml, "noux");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libc_noux.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "bash-minimal.tar");
		gen_parent_rom_route(xml, "coreutils-minimal.tar");
		gen_parent_rom_route(xml, "vim-minimal.tar");
		gen_parent_rom_route(xml, "ncurses.lib.so");
		gen_parent_rom_route(xml, "posix.lib.so");
		gen_parent_rom_route(xml, "depot_users.tar");
		gen_parent_rom_route(xml, "vimrc", "config -> vimrc");
		gen_parent_rom_route(xml, "VERSION");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);

		gen_service_node<::File_system::Session>(xml, [&] () {
			xml.attribute("label", "report");
			xml.node("parent", [&] () { xml.attribute("label", "report"); });
		});

		for_each_inspected_storage_target(devices, [&] (Storage_target const &target) {
			gen_service_node<::File_system::Session>(xml, [&] () {
				xml.attribute("label", target.label());
				gen_named_node(xml, "child", target.fs());
			});
		});

		if (ram_fs_state.inspected)
			gen_service_node<::File_system::Session>(xml, [&] () {
				xml.attribute("label", "ram");
				gen_named_node(xml, "child", "ram_fs");
			});
	});
}


void Sculpt::gen_file_browser(Xml_generator &xml,
                              Storage_devices const &devices,
                              Ram_fs_state const &ram_fs_state,
                              File_browser_version version)
{
	xml.node("start", [&] () {
		gen_nit_fb_start(xml, "inspect"); });

	xml.node("start", [&] () {
		gen_terminal_start(xml, "inspect terminal", "inspect",
		                   version); });

	xml.node("start", [&] () {
		gen_noux_start(xml, "inspect noux", "inspect terminal",
		               devices, ram_fs_state, version); });
}
