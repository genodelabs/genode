/*
 * \brief  Sculpt Wifi-driver management
 * \author Norman Feske
 * \date   2024-03-26
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER__WIFI_H_
#define _DRIVER__WIFI_H_

namespace Sculpt { struct Wifi_driver; }


struct Sculpt::Wifi_driver : private Noncopyable
{
	Constructible<Child_state> _wifi { };

	void gen_start_node(Generator &g) const
	{
		if (!_wifi.constructed())
			return;

		g.node("start", [&] {
			_wifi->gen_start_node_content(g);
			gen_named_node(g, "binary", "wifi");

			g.node("config", [&] {
				g.attribute("dtb", "wifi.dtb");

				g.node("vfs", [&] {
					gen_named_node(g, "dir", "dev", [&] {
						g.node("null", [&] {});
						g.node("zero", [&] {});
						g.node("log",  [&] {});
						g.node("null", [&] {});
						gen_named_node(g, "jitterentropy", "random");
						gen_named_node(g, "jitterentropy", "urandom"); });
						gen_named_node(g, "inline", "rtc", [&] {
							g.append_quoted("2018-01-01 00:01");
						});
					gen_named_node(g, "dir", "firmware", [&] {
						g.node("tar", [&] {
							g.attribute("name", "wifi_firmware.tar");
						});
					});
				});

				g.node("libc", [&] {
					g.attribute("stdout", "/dev/log");
					g.attribute("stderr", "/dev/null");
					g.attribute("rtc",    "/dev/rtc");
				});
			});

			g.tabular_node("route", [&] {
				gen_service_node<Platform::Session>(g, [&] {
					g.node("parent", [&] {
						g.attribute("label", "wifi"); }); });
				g.node("service", [&] {
					g.attribute("name", "Uplink");
					g.node("child", [&] {
						g.attribute("name", "nic_router");
						g.attribute("label", "wifi -> "); }); });
				gen_common_routes(g);
				gen_parent_rom_route(g, "wifi");
				gen_parent_rom_route(g, "wifi.dtb");
				gen_parent_rom_route(g, "libcrypto.lib.so");
				gen_parent_rom_route(g, "vfs.lib.so");
				gen_parent_rom_route(g, "libc.lib.so");
				gen_parent_rom_route(g, "libm.lib.so");
				gen_parent_rom_route(g, "vfs_jitterentropy.lib.so");
				gen_parent_rom_route(g, "libssl.lib.so");
				gen_parent_rom_route(g, "wifi.lib.so");
				gen_parent_rom_route(g, "wifi_firmware.tar");
				gen_parent_rom_route(g, "wpa_driver_nl80211.lib.so");
				gen_parent_rom_route(g, "wpa_supplicant.lib.so");
				gen_parent_route<Rm_session>   (g);
				gen_parent_route<Rtc::Session> (g);
				gen_service_node<Rom_session>(g, [&] {
					g.attribute("label", "wifi_config");
					g.node("parent", [&] {
						g.attribute("label", "config -> managed/wifi"); }); });
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		bool const use_wifi = board_info.wifi_avail()
		                  &&  board_info.options.wifi
		                  && !board_info.options.suspending;

		_wifi.conditional(use_wifi, registry, "wifi", Priority::DEFAULT,
		                  Ram_quota { 16*1024*1024 }, Cap_quota { 260 });
	}
};

#endif /* _DRIVER__WIFI_H_ */
