/*
 * \brief  XML configuration for USB network driver
 * \author Norman Feske
 * \date   2022-08-24
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <runtime.h>

void Sculpt::gen_usb_net_start_content(Xml_generator &xml)
{
	gen_common_start_content(xml, "usb_net",
	                         Cap_quota{200}, Ram_quota{20*1024*1024},
	                         Priority::NETWORK);

	xml.node("binary", [&] () {
		xml.attribute("name", "usb_net_drv");
	});

	xml.node("config", [&] () {
		xml.attribute("mac", "02:00:00:00:01:05");
	});

	xml.node("route", [&] () {

		xml.node("service", [&] () {
			xml.attribute("name", "Uplink");
			xml.node("child", [&] () {
				xml.attribute("name", "nic_router");
				xml.attribute("label", "usb_net -> ");
			});
		});

		gen_service_node<Usb::Session>(xml, [&] () {
			xml.node("parent", [&] () {
				xml.attribute("label", "usb_net"); }); });

		gen_parent_rom_route(xml, "usb_net_drv");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>   (xml);
		gen_parent_route<Pd_session>    (xml);
		gen_parent_route<Rm_session>    (xml);
		gen_parent_route<Log_session>   (xml);
		gen_parent_route<Timer::Session>(xml);
	});
}
