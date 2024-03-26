/*
 * \brief  XML configuration for fs_tool
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

void Sculpt::gen_fs_tool_start_content(Xml_generator &xml, Fs_tool_version version,
                                       File_operation_queue const &operations)
{
	xml.attribute("version", version.value);

	gen_common_start_content(xml, "fs_tool", Cap_quota{200}, Ram_quota{5*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "fs_tool");

	xml.node("config", [&] {

		xml.attribute("exit",    "yes");
		xml.attribute("verbose", "yes");

		xml.node("vfs", [&] {

			auto gen_fs = [&] (auto name, auto label, auto buffer_size)
			{
				gen_named_node(xml, "dir", name, [&] {
					xml.node("fs",  [&] {
						xml.attribute("label",       label);
						xml.attribute("buffer_size", buffer_size); }); });
			};

			gen_fs("rw",     "target", "1M");
			gen_fs("config", "config", "128K");
		});

		operations.gen_fs_tool_config(xml);
	});

	xml.node("route", [&] {

		gen_service_node<::File_system::Session>(xml, [&] {
			xml.attribute("label", "target");
			gen_named_node(xml, "child", "default_fs_rw"); });

		gen_parent_rom_route(xml, "fs_tool");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_route<Cpu_session> (xml);
		gen_parent_route<Pd_session>  (xml);
		gen_parent_route<Log_session> (xml);
		gen_parent_route<Rom_session> (xml);

		gen_service_node<::File_system::Session>(xml, [&] {
			xml.attribute("label", "config");
			xml.node("parent", [&] { xml.attribute("label", "config"); }); });
	});
}
