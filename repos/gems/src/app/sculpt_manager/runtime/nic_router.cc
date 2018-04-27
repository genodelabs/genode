/*
 * \brief  XML configuration for the NIC router
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


void Sculpt::gen_nic_router_start_content(Xml_generator &xml)
{
	gen_common_start_content(xml, "nic_router",
	                         Cap_quota{300}, Ram_quota{10*1024*1024});

	gen_provides<Nic::Session>(xml);

	xml.node("route", [&] () {
		gen_parent_rom_route(xml, "nic_router");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "config", "config -> managed/nic_router");
		gen_parent_route<Cpu_session>     (xml);
		gen_parent_route<Pd_session>      (xml);
		gen_parent_route<Rm_session>      (xml);
		gen_parent_route<Log_session>     (xml);
		gen_parent_route<Timer::Session>  (xml);
		gen_parent_route<Report::Session> (xml);
		gen_service_node<Nic::Session>(xml, [&] () {
			xml.attribute("label", "wired");
			gen_named_node(xml, "child", "nic_drv");
		});
		gen_service_node<Nic::Session>(xml, [&] () {
			xml.attribute("label", "wifi");
			gen_named_node(xml, "child", "wifi_drv");
		});
	});
}
