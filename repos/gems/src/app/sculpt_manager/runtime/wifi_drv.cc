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
	gen_common_start_content(xml, "wifi_drv", Cap_quota{300}, Ram_quota{54*1024*1024});

	gen_provides<Nic::Session>(xml);

	xml.node("config", [&] () {
		xml.attribute("connected_scan_interval", "0");
		xml.attribute("use_11n", "no");

		xml.node("vfs", [&] () {
			gen_named_node(xml, "dir", "dev", [&] () {
				xml.node("null", [&] () {});
				xml.node("zero", [&] () {});
				xml.node("rtc",  [&] () {});
				xml.node("log",  [&] () {});
				gen_named_node(xml, "jitterentropy", "random");
				gen_named_node(xml, "jitterentropy", "urandom"); });

			gen_named_node(xml, "dir", "config", [&] () {
				xml.node("ram", [&] () {}); });
		});

		xml.node("libc", [&] () {
			xml.attribute("stdout", "/dev/null");
			xml.attribute("stderr", "/dev/log");
			xml.attribute("rtc",    "/dev/rtc");
		});
	});

	xml.node("route", [&] () {
		gen_parent_rom_route(xml, "wifi_drv");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "libcrypto.lib.so");
		gen_parent_rom_route(xml, "vfs.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "vfs_jitterentropy.lib.so");
		gen_parent_rom_route(xml, "libssl.lib.so");
		gen_parent_rom_route(xml, "wifi.lib.so");
		gen_parent_rom_route(xml, "wpa_driver_nl80211.lib.so");
		gen_parent_rom_route(xml, "wpa_supplicant.lib.so");
		gen_parent_rom_route(xml, "iwlwifi-1000-5.ucode");
		gen_parent_rom_route(xml, "iwlwifi-3160-16.ucode");
		gen_parent_rom_route(xml, "iwlwifi-6000-4.ucode");
		gen_parent_rom_route(xml, "iwlwifi-6000g2a-6.ucode");
		gen_parent_rom_route(xml, "iwlwifi-6000g2b-6.ucode");
		gen_parent_rom_route(xml, "iwlwifi-7260-16.ucode");
		gen_parent_rom_route(xml, "iwlwifi-7265-16.ucode");
		gen_parent_rom_route(xml, "iwlwifi-7265D-16.ucode");
		gen_parent_rom_route(xml, "iwlwifi-8000C-16.ucode");
		gen_parent_route<Cpu_session>      (xml);
		gen_parent_route<Pd_session>       (xml);
		gen_parent_route<Rm_session>       (xml);
		gen_parent_route<Log_session>      (xml);
		gen_parent_route<Timer::Session>   (xml);
		gen_parent_route<Rtc::Session>     (xml);
		gen_parent_route<Report::Session>  (xml);

		gen_service_node<Rom_session>(xml, [&] () {
			xml.attribute("label", "wlan_configuration");
			xml.node("parent", [&] () {
				xml.attribute("label", "config -> managed/wlan"); }); });

		gen_service_node<Platform::Session>(xml, [&] () {
			xml.node("parent", [&] () {
				xml.attribute("label", "wifi"); }); });
	});
}
