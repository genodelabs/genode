/*
 * \brief  XML configuration for the fs-query tool for obtaining the launchers
 * \author Norman Feske
 * \date   2018-08-21
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_launcher_query_start_content(Xml_generator &xml)
{
	gen_common_start_content(xml, "launcher_query",
	                         Cap_quota{200}, Ram_quota{2*1024*1024},
	                         Priority::STORAGE);

	gen_named_node(xml, "binary", "fs_query");

	xml.node("config", [&] {
		xml.attribute("query", "rom");
		xml.node("vfs", [&] {
			xml.node("fs", [&] {}); });

		xml.node("query", [&] {
			xml.attribute("path", "/launcher");
			xml.attribute("content", "yes");
		});

		xml.node("query", [&] {
			xml.attribute("path", "/presets");
			xml.attribute("content", "yes");
		});
	});

	xml.node("route", [&] {
		gen_parent_rom_route(xml, "fs_query");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");

		gen_parent_route<Cpu_session>     (xml);
		gen_parent_route<Pd_session>      (xml);
		gen_parent_route<Log_session>     (xml);
		gen_parent_route<Report::Session> (xml);

		gen_service_node<::File_system::Session>(xml, [&] {
			xml.node("parent", [&] {
				xml.attribute("label", "config"); }); });
	});
}
