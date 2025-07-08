/*
 * \brief  Sculpt USB-driver management
 * \author Norman Feske
 * \date   2024-03-18
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER__USB_H_
#define _DRIVER__USB_H_

#include <managed_config.h>

namespace Sculpt { struct Usb_driver; }


struct Sculpt::Usb_driver : private Noncopyable
{
	struct Action : Interface
	{
		virtual void handle_usb_plug_unplug() = 0;
	};

	struct Info : Interface
	{
		virtual void gen_usb_storage_policies(Generator &) const = 0;
	};

	Env        &_env;
	Info const &_info;
	Action     &_action;

	Constructible<Child_state> _hcd { }, _hid { }, _net { };

	static constexpr unsigned CLASS_HID = 3, CLASS_NET = 2, CLASS_STORAGE = 8;

	struct Detected
	{
		bool hid, net;
		bool storage_aquired;

		static Detected from_node(Node const &devices)
		{
			Detected result { };
			devices.for_each_sub_node("device", [&] (Node const &device) {
				bool const acquired = device.attribute_value("acquired", false);
				device.for_each_sub_node("config", [&] (Node const &config) {
					config.for_each_sub_node("interface", [&] (Node const &interface) {
						unsigned const class_id = interface.attribute_value("class", 0u);
						result.hid             |= (class_id == CLASS_HID);
						result.net             |= (class_id == CLASS_NET);
						result.storage_aquired |= (class_id == CLASS_STORAGE) && acquired;
					});
				});
			});
			return result;
		}

	} _detected { };

	Rom_handler<Usb_driver> _devices {
		_env, "report -> runtime/usb/devices", *this, &Usb_driver::_handle_devices };

	void _handle_devices(Node const &devices)
	{
		_detected = Detected::from_node(devices);
		_action.handle_usb_plug_unplug();
	}

	Managed_config<Usb_driver> _usb_config {
		_env, "config", "usb", *this, &Usb_driver::_handle_usb_config };

	void _handle_usb_config(Node const &config)
	{
		_usb_config.generate([&] (Generator &g) {
			g.node_attributes(config);

			g.node("report", [&] {
				g.attribute("devices", "yes"); });

			g.node("policy", [&] {
				g.attribute("label_prefix", "usb_hid");
				g.node("device", [&] {
					g.attribute("class", CLASS_HID); }); });

			/* copy user-provided rules */
			config.for_each_sub_node("policy", [&] (Node const &policy) {
				(void)g.append_node(policy, Generator::Max_depth { 5 }); });

			/* wildcard for USB clients with no policy yet */
			g.node("default-policy", [&] { });

			_info.gen_usb_storage_policies(g);
		});
	}

	Usb_driver(Env &env, Info const &info, Action &action)
	:
		_env(env), _info(info), _action(action)
	{
		_usb_config.trigger_update();
	}

	void gen_start_nodes(Generator &g) const
	{
		auto start_node = [&] (auto const &driver, auto const &binary, auto const &fn)
		{
			if (driver.constructed())
				g.node("start", [&] {
					driver->gen_start_node_content(g);
					gen_named_node(g, "binary", binary);
					fn(); });
		};

		start_node(_hcd, "usb", [&] {
			gen_provides<Usb::Session>(g);
			g.node("route", [&] {
				gen_parent_route<Platform::Session>(g);
				gen_parent_rom_route(g, "usb");
				gen_parent_rom_route(g, "config", "config -> managed/usb");
				gen_parent_rom_route(g, "dtb",    "usb.dtb");
				gen_common_routes(g);
			});
		});

		start_node(_hid, "usb_hid", [&] {
			g.node("config", [&] {
				g.attribute("capslock_led", "rom");
				g.attribute("numlock_led",  "rom");
			});
			g.node("route", [&] {
				gen_service_node<Usb::Session>(g, [&] {
					gen_named_node(g, "child", "usb"); });
				gen_parent_rom_route(g, "usb_hid");
				gen_parent_rom_route(g, "capslock", "capslock");
				gen_parent_rom_route(g, "numlock",  "numlock");
				gen_common_routes(g);
				gen_service_node<Event::Session>(g, [&] {
					g.node("parent", [&] {
						g.attribute("label", "usb_hid"); }); });
			});
		});

		start_node(_net, "usb_net", [&] {
			g.node("config", [&] {
				g.attribute("mac", "02:00:00:00:01:05");
			});
			g.node("route", [&] {
				gen_service_node<Usb::Session>(g, [&] {
					gen_named_node(g, "child", "usb"); });
				gen_parent_rom_route(g, "usb_net");
				gen_common_routes(g);
				gen_service_node<Uplink::Session>(g, [&] {
					g.node("child", [&] {
						g.attribute("name", "nic_router");
						g.attribute("label", "usb_net -> ");
					});
				});
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		bool const suspending = board_info.options.suspending;

		_hcd.conditional(board_info.usb_avail() && !suspending,
		                 registry, "usb", Priority::MULTIMEDIA,
		                 Ram_quota { 16*1024*1024 }, Cap_quota { 200 });

		_hid.conditional(board_info.usb_avail() && _detected.hid && !suspending,
		                 registry, "usb_hid", Priority::MULTIMEDIA,
		                 Ram_quota { 11*1024*1024 }, Cap_quota { 180 });

		_net.conditional(board_info.usb_avail() && _detected.net && !suspending && board_info.options.usb_net,
		                 registry, "usb_net", Priority::DEFAULT,
		                 Ram_quota { 20*1024*1024 }, Cap_quota { 200 });

		_usb_config.trigger_update();
	}

	void with_devices(auto const &fn) const
	{
		_devices.with_node([&] (Node const &devices) {
			fn({ .present = _hcd.constructed(), .report = devices }); });
	}

	bool suspend_supported() const { return !_detected.storage_aquired; }
};

#endif /* _DRIVER__USB_H_ */
