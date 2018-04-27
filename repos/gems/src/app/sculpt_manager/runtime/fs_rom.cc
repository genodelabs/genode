/*
 * \brief  XML configuration for the fs-rom component
 * \author Norman Feske
 * \date   2018-05-09
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_fs_rom_start_content(Xml_generator &xml,
                                      Start_name const &name,
                                      Start_name const &server,
                                      Ram_quota ram_quota)
{
	gen_common_start_content(xml, name,
	                         Cap_quota{200}, ram_quota);

	gen_named_node(xml, "binary", "fs_rom");

	xml.node("config", [&] () { });

	gen_provides<Rom_session>(xml);

	xml.node("route", [&] () {

		gen_service_node<::File_system::Session>(xml, [&] () {
			gen_named_node(xml, "child", server); });

		gen_parent_rom_route(xml, "fs_rom");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>(xml);
		gen_parent_route<Pd_session> (xml);
		gen_parent_route<Log_session>(xml);
	});
}
