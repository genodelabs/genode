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
	Allocator  &_alloc;
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
		_env, "report -> usb/devices", *this, &Usb_driver::_handle_devices };

	void _handle_devices(Node const &devices)
	{
		_detected = Detected::from_node(devices);
		_action.handle_usb_plug_unplug();
	}

	Managed_config<Usb_driver> _usb_config {
		_env, _alloc, "config", "usb", *this, &Usb_driver::_handle_usb_config };

	void _handle_usb_config(Node const &config)
	{
		_usb_config.generate([&] (Generator &g) {
			config.for_each_attribute([&] (Node::Attribute const &a) {
				if (a.name != "managed")
					g.attribute(a.name.string(), a.value.start, a.value.num_bytes); });

			g.node("report", [&] {
				g.attribute("devices", "yes"); });

			g.node("policy", [&] {
				g.attribute("label_prefix", "usb_hid");
				g.attribute("generated", "yes");
				g.node("device", [&] {
					g.attribute("class", CLASS_HID); }); });

			/* copy user-provided rules */
			config.for_each_sub_node("policy", [&] (Node const &policy) {
				if (!policy.attribute_value("generated", false))
					(void)g.append_node(policy, Generator::Max_depth { 5 }); });

			/* wildcard for USB clients with no policy yet */
			g.node("default-policy", [&] { });

			_info.gen_usb_storage_policies(g);
		});
	}

	Usb_driver(Env &env, Allocator &alloc, Info const &info, Action &action)
	:
		_env(env), _alloc(alloc), _info(info), _action(action)
	{
		_usb_config.trigger_update();
	}

	void gen_child_nodes(Generator &g) const
	{
		auto child_node = [&] (auto const &driver, auto const &fn)
		{
			if (driver.constructed())
				g.node("child", [&] {
					driver->gen_child_node_content(g);
					fn(); });
		};

		child_node(_hcd, [&] {
			g.node("provides", [&] { g.node("usb"); });
			g.tabular_node("connect", [&] {
				connect_platform(g, "usb");
				connect_report(g);
				connect_config_rom(g, "config", "usb");
				connect_parent_rom(g, "dtb",    "usb.dtb");
			});
		});

		child_node(_hid, [&] {
			g.node("config", [&] {
				g.attribute("capslock_led", "rom");
				g.attribute("numlock_led",  "rom");
			});
			g.tabular_node("connect", [&] {
				g.node("usb", [&] {
					gen_named_node(g, "child", "usb"); });
				connect_report(g);
				connect_report_rom(g, "capslock", "global_keys/capslock");
				connect_report_rom(g, "numlock",  "global_keys/numlock");
				connect_event(g, "usb_hid");
			});
		});

		child_node(_net, [&] {
			g.node("config", [&] {
				g.node("device", [&] {
					/*
					 * Only set for * Quectel EG-25g, setting the MAC-address doesn't work
					 * for all devices, so better stick to the defaults
					 */
					g.attribute("vendor_id",  "0x2c7c");
					g.attribute("product_id", "0x0125");
					g.attribute("mac", "02:00:00:00:01:05");
				});
			});
			g.tabular_node("connect", [&] {
				g.node("usb", [&] {
					gen_named_node(g, "child", "usb"); });
				connect_report(g);
				g.node("uplink", [&] {
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
		                 registry, Priority::MULTIMEDIA,
		                 Child_name { "usb" }, Binary_name { "usb" },
		                 Ram_quota { 16*1024*1024 }, Cap_quota { 200 });

		_hid.conditional(board_info.usb_avail() && _detected.hid && !suspending,
		                 registry, Priority::MULTIMEDIA,
		                 Child_name { "usb_hid" }, Binary_name { "usb_hid" },
		                 Ram_quota { 11*1024*1024 }, Cap_quota { 180 });

		_net.conditional(board_info.usb_avail() && _detected.net && !suspending && board_info.options.usb_net,
		                 registry, Priority::DEFAULT,
		                 Child_name { "usb_net" }, Binary_name { "usb_net" },
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
