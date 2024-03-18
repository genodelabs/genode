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

#ifndef _USB_DRIVER_H_
#define _USB_DRIVER_H_

/* Genode includes */
#include <uplink_session/uplink_session.h>

/* local includes */
#include <model/child_exit_state.h>
#include <model/board_info.h>
#include <managed_config.h>
#include <runtime.h>

namespace Sculpt { struct Usb_driver; }


struct Sculpt::Usb_driver : private Noncopyable
{
	struct Action : Interface
	{
		virtual void handle_usb_plug_unplug() = 0;
		virtual void gen_usb_storage_policies(Xml_generator &) const = 0;
	};

	Env    &_env;
	Action &_action;

	Constructible<Child_state> _hcd { },
	                           _hid { },
	                           _net { };

	Attached_rom_dataspace _devices { _env, "report -> runtime/usb/devices" };

	Signal_handler<Usb_driver> _devices_handler {
		_env.ep(), *this, &Usb_driver::_handle_devices };

	void _handle_devices()
	{
		_devices.update();
		_action.handle_usb_plug_unplug();
	}

	Managed_config<Usb_driver> _usb_config {
		_env, "config", "usb", *this, &Usb_driver::_handle_usb_config };

	void _handle_usb_config(Xml_node config)
	{
		_usb_config.generate([&] (Xml_generator &xml) {
			copy_attributes(xml, config);

			xml.node("report", [&] {
				xml.attribute("devices", "yes"); });

			xml.node("policy", [&] {
				xml.attribute("label_prefix", "usb_hid");
				xml.node("device", [&] {
					xml.attribute("class", "0x3"); }); });

			/* copy user-provided rules */
			config.for_each_sub_node("policy", [&] (Xml_node const &policy) {
				copy_node(xml, policy); });

			_action.gen_usb_storage_policies(xml);
		});
	}

	Usb_driver(Env &env, Action &action) : _env(env), _action(action)
	{
		_devices.sigh(_devices_handler);
		_usb_config.trigger_update();
		_devices_handler.local_submit();
	}

	void gen_start_nodes(Xml_generator &xml) const
	{
		auto gen_common_routes = [&] (Xml_generator &xml)
		{
			gen_parent_route<Rom_session>     (xml);
			gen_parent_route<Cpu_session>     (xml);
			gen_parent_route<Pd_session>      (xml);
			gen_parent_route<Log_session>     (xml);
			gen_parent_route<Timer::Session>  (xml);
			gen_parent_route<Report::Session> (xml);
		};

		auto start_node = [&] (auto const &driver, auto const &binary, auto const &fn)
		{
			if (driver.constructed())
				xml.node("start", [&] {
					driver->gen_start_node_content(xml);
					gen_named_node(xml, "binary", binary);
					fn(); });
		};

		start_node(_hcd, "usb_drv", [&] {
			gen_provides<Usb::Session>(xml);
			xml.node("route", [&] {
				gen_parent_route<Platform::Session>(xml);
				gen_parent_rom_route(xml, "config", "config -> managed/usb");
				gen_parent_rom_route(xml, "dtb",    "usb_drv.dtb");
				gen_common_routes(xml);
			});
		});

		start_node(_hid, "usb_hid_drv", [&] {
			xml.node("config", [&] {
				xml.attribute("capslock_led", "rom");
				xml.attribute("numlock_led",  "rom");
			});
			xml.node("route", [&] {
				gen_service_node<Usb::Session>(xml, [&] {
					gen_named_node(xml, "child", "usb"); });
				gen_parent_rom_route(xml, "capslock", "capslock");
				gen_parent_rom_route(xml, "numlock",  "numlock");
				gen_common_routes(xml);
				gen_service_node<Event::Session>(xml, [&] {
					xml.node("parent", [&] {
						xml.attribute("label", "usb_hid"); }); });
			});
		});

		start_node(_net, "usb_net_drv", [&] {
			xml.node("config", [&] {
				xml.attribute("mac", "02:00:00:00:01:05");
			});
			xml.node("route", [&] {
				gen_service_node<Usb::Session>(xml, [&] {
					gen_named_node(xml, "child", "usb"); });
				gen_common_routes(xml);
				gen_service_node<Uplink::Session>(xml, [&] {
					xml.node("child", [&] () {
						xml.attribute("name", "nic_router");
						xml.attribute("label", "usb_net -> ");
					});
				});
			});
		});
	};

	struct Features { bool hid, net; };

	void update(Registry<Child_state> &registry,
	            Board_info const &board_info, Features features)
	{
		_hcd.conditional(board_info.usb_present,
		                 registry, "usb", Priority::MULTIMEDIA,
		                 Ram_quota { 16*1024*1024 }, Cap_quota { 200 });

		_hid.conditional(board_info.usb_present && features.hid,
		                 registry, "usb_hid", Priority::MULTIMEDIA,
		                 Ram_quota { 11*1024*1024 }, Cap_quota { 180 });

		_net.conditional(board_info.usb_present && features.net,
		                 registry, "usb_net", Priority::DEFAULT,
		                 Ram_quota { 20*1024*1024 }, Cap_quota { 200 });

		_usb_config.trigger_update();
	}

	void with_devices(auto const &fn) const { fn(_devices.xml()); }
};

#endif /* _USB_DRIVER_H_ */
