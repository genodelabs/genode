/*
 * \brief  XML configuration for RAM file system
 * \author Norman Feske
 * \date   2018-05-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_ram_fs_start_content(Xml_generator &xml,
                                      Ram_fs_state const &state)
{
	xml.attribute("version", state.version.value);

	gen_common_start_content(xml, "ram_fs", state.cap_quota, state.ram_quota);

	gen_provides<::File_system::Session>(xml);

	xml.node("config", [&] () {
		xml.node("default-policy", [&] () {
			xml.attribute("root",      "/");
			xml.attribute("writeable", "yes");
		});
	});

	xml.node("route", [&] () {
		gen_parent_rom_route(xml, "ram_fs");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session> (xml);
		gen_parent_route<Pd_session>  (xml);
		gen_parent_route<Log_session> (xml);
	});
}
