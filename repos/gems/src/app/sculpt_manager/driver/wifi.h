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

	void gen_child_node(Generator &g) const
	{
		if (!_wifi.constructed())
			return;

		g.node("child", [&] {
			_wifi->gen_child_node_content(g);

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

			g.tabular_node("connect", [&] {
				connect_platform(g, "wifi");
				g.node("uplink", [&] {
					g.node("child", [&] {
						g.attribute("name", "nic_router");
						g.attribute("label", "wifi -> "); }); });
				connect_report(g);
				connect_parent_rom(g, "wifi.dtb");
				connect_parent_rom(g, "libcrypto.lib.so");
				connect_parent_rom(g, "vfs.lib.so");
				connect_parent_rom(g, "libc.lib.so");
				connect_parent_rom(g, "libm.lib.so");
				connect_parent_rom(g, "vfs_jitterentropy.lib.so");
				connect_parent_rom(g, "libssl.lib.so");
				connect_parent_rom(g, "wifi.lib.so");
				connect_parent_rom(g, "wifi_firmware.tar");
				connect_parent_rom(g, "wpa_driver_nl80211.lib.so");
				connect_parent_rom(g, "wpa_supplicant.lib.so");
				g.node("rm",  [&] { g.node("parent"); });
				g.node("rtc", [&] { g.node("parent"); });
				connect_config_rom(g, "wifi_config", "wifi");
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		bool const use_wifi = board_info.wifi_avail()
		                  &&  board_info.options.wifi
		                  && !board_info.options.suspending;

		_wifi.conditional(use_wifi, registry, Priority::DEFAULT,
		                  Child_name { "wifi" }, Binary_name { "wifi" },
		                  Ram_quota { 16*1024*1024 }, Cap_quota { 260 });
	}
};

#endif /* _DRIVER__WIFI_H_ */
