/*
 * \brief  XML configuration for wired NIC driver
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
#include <uplink_session/uplink_session.h>

void Sculpt::gen_nic_drv_start_content(Xml_generator &xml)
{
	gen_common_start_content(xml, "nic_drv", Cap_quota{300}, Ram_quota{16*1024*1024});
	gen_named_node(xml, "resource", "CPU", [&] () { xml.attribute("quantum", "50"); });

	xml.node("config", [&] () { xml.attribute("mode", "uplink_client"); });

	xml.node("route", [&] () {

		gen_service_node<Uplink::Session>(xml, [&] () {
			xml.node("child", [&] () {
				xml.attribute("name", "nic_router"); }); });

		gen_service_node<Platform::Session>(xml, [&] () {
			xml.node("parent", [&] () {
				xml.attribute("label", "nic"); }); });

		gen_parent_rom_route(xml, "nic_drv");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Rm_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
	});
}
