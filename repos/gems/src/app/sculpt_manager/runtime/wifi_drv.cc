/*
 * \brief  XML configuration for wireless driver
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

void Sculpt::gen_wifi_drv_start_content(Xml_generator &xml)
{
	gen_common_start_content(xml, "wifi_drv",
	                         Cap_quota{250}, Ram_quota{32*1024*1024},
	                         Priority::NETWORK);

	xml.node("config", [&] () {

		xml.attribute("dtb", "wifi_drv.dtb");

		xml.node("vfs", [&] () {
			gen_named_node(xml, "dir", "dev", [&] () {
				xml.node("null", [&] () {});
				xml.node("zero", [&] () {});
				xml.node("log",  [&] () {});
				xml.node("null", [&] () {});
				gen_named_node(xml, "jitterentropy", "random");
				gen_named_node(xml, "jitterentropy", "urandom"); });
				gen_named_node(xml, "inline", "rtc", [&] () {
					xml.append("2018-01-01 00:01");
				});
			gen_named_node(xml, "dir", "firmware", [&] () {
				xml.node("tar", [&] () {
					xml.attribute("name", "wifi_firmware.tar");
				});
			});
		});

		xml.node("libc", [&] () {
			xml.attribute("stdout", "/dev/null");
			xml.attribute("stderr", "/dev/null");
			xml.attribute("rtc",    "/dev/rtc");
		});
	});

	xml.node("route", [&] () {

		xml.node("service", [&] () {
			xml.attribute("name", "Uplink");
			xml.node("child", [&] () {
				xml.attribute("name", "nic_router");
				xml.attribute("label", "wifi_drv -> ");
			});
		});

		gen_service_node<Platform::Session>(xml, [&] () {
			xml.node("parent", [&] () {
				xml.attribute("label", "wifi"); }); });

		gen_parent_rom_route(xml, "wifi_drv");
		gen_parent_rom_route(xml, "wifi_drv.dtb");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "libcrypto.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "vfs_jitterentropy.lib.so");
		gen_parent_rom_route(xml, "libssl.lib.so");
		gen_parent_rom_route(xml, "wifi.lib.so");
		gen_parent_rom_route(xml, "wifi_firmware.tar");
		gen_parent_rom_route(xml, "wpa_driver_nl80211.lib.so");
		gen_parent_rom_route(xml, "wpa_supplicant.lib.so");
		gen_parent_route<Cpu_session>      (xml);
		gen_parent_route<Pd_session>       (xml);
		gen_parent_route<Rm_session>       (xml);
		gen_parent_route<Log_session>      (xml);
		gen_parent_route<Timer::Session>   (xml);
		gen_parent_route<Rtc::Session>     (xml);
		gen_parent_route<Report::Session>  (xml);

		gen_service_node<Rom_session>(xml, [&] () {
			xml.attribute("label", "wifi_config");
			xml.node("parent", [&] () {
				xml.attribute("label", "config -> managed/wifi"); }); });
	});
}
