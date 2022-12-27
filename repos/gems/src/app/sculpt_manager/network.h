/*
 * \brief  Sculpt network management
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

/* local includes */
#include <model/child_exit_state.h>
#include <model/pci_info.h>
#include <view/network_dialog.h>
#include <menu_view.h>
#include <runtime.h>
#include <keyboard_focus.h>
#include <managed_config.h>

namespace Sculpt { struct Network; }


struct Sculpt::Network : Network_dialog::Action
{
	Env &_env;

	Allocator &_alloc;

	struct Action : Interface
	{
		virtual void update_network_dialog() = 0;
	};

	Action &_action;

	Registry<Child_state> &_child_states;

	Runtime_config_generator &_runtime_config_generator;

	Runtime_info const &_runtime_info;
	Pci_info     const &_pci_info;

	Nic_target _nic_target { };
	Nic_state  _nic_state  { };

	Wpa_passphrase wpa_passphrase { };

	unsigned _nic_drv_version  = 0;
	unsigned _wifi_drv_version = 0;
	unsigned _usb_net_version  = 0;

	Attached_rom_dataspace _wlan_accesspoints_rom {
		_env, "report -> runtime/wifi_drv/accesspoints" };

	Attached_rom_dataspace _wlan_state_rom {
		_env, "report -> runtime/wifi_drv/state" };

	Attached_rom_dataspace _nic_router_state_rom {
		_env, "report -> runtime/nic_router/state" };

	void _generate_nic_router_config();

	void _generate_nic_router_uplink(Xml_generator &xml,
	                                 char    const *label);

	Access_points _access_points { };

	Wifi_connection _wifi_connection = Wifi_connection::disconnected_wifi_connection();

	void gen_runtime_start_nodes(Xml_generator &xml) const;

	bool ready() const { return _nic_target.ready() && _nic_state.ready(); }

	void handle_key_press(Codepoint);

	void _handle_wlan_accesspoints();
	void _handle_wlan_state();
	void _handle_nic_router_state();
	void _handle_nic_router_config(Xml_node);

	Managed_config<Network> _nic_router_config {
		_env, "config", "nic_router", *this, &Network::_handle_nic_router_config };

	Signal_handler<Network> _wlan_accesspoints_handler {
		_env.ep(), *this, &Network::_handle_wlan_accesspoints };

	Signal_handler<Network> _wlan_state_handler {
		_env.ep(), *this, &Network::_handle_wlan_state };

	Signal_handler<Network> _nic_router_state_handler {
		_env.ep(), *this, &Network::_handle_nic_router_state };

	Network_dialog::Wlan_config_policy _wlan_config_policy =
		Network_dialog::WLAN_CONFIG_MANAGED;

	Network_dialog dialog {
		_nic_target, _access_points,
		_wifi_connection, _nic_state, wpa_passphrase, _wlan_config_policy,
		_pci_info };

	Managed_config<Network> _wlan_config {
		_env, "config", "wifi", *this, &Network::_handle_wlan_config };

	void _handle_wlan_config(Xml_node)
	{
		if (_wlan_config.try_generate_manually_managed()) {
			_wlan_config_policy = Network_dialog::WLAN_CONFIG_MANUAL;
			_action.update_network_dialog();
			return;
		}

		_wlan_config_policy = Network_dialog::WLAN_CONFIG_MANAGED;;

		if (_wifi_connection.connected())
			wifi_connect(_wifi_connection.bssid);
		else
			wifi_disconnect();
	}

	void _update_nic_target_from_config(Xml_node const &);

	/**
	 * Network_dialog::Action interface
	 */
	void nic_target(Nic_target::Type const type) override
	{
		if (type != _nic_target.managed_type) {
			_nic_target.managed_type = type;
			_generate_nic_router_config();
			_runtime_config_generator.generate_runtime_config();
			_action.update_network_dialog();
		}
	}

	void wifi_connect(Access_point::Bssid bssid) override
	{
		_access_points.for_each([&] (Access_point const &ap) {
			if (ap.bssid != bssid)
				return;

			_wifi_connection.ssid  = ap.ssid;
			_wifi_connection.bssid = ap.bssid;
			_wifi_connection.state = Wifi_connection::CONNECTING;

			_wlan_config.generate([&] (Xml_generator &xml) {

				xml.attribute("connected_scan_interval", 0U);
				xml.attribute("scan_interval", 10U);
				xml.attribute("use_11n", false);

				xml.attribute("verbose_state", false);
				xml.attribute("verbose",       false);

				xml.node("network", [&]() {
					xml.attribute("ssid", ap.ssid);

					/* for now always try to use WPA2 */
					if (ap.protection == Access_point::WPA_PSK) {
						xml.attribute("protection", "WPA2");
						String<128> const psk(wpa_passphrase);
						xml.attribute("passphrase", psk);
					}
				});
			});
		});
	}

	void restart_nic_drv_on_next_runtime_cfg()  { _nic_drv_version++; }
	void restart_wifi_drv_on_next_runtime_cfg() { _wifi_drv_version++; }
	void restart_usb_net_on_next_runtime_cfg()  { _usb_net_version++; }

	void wifi_disconnect() override
	{
		/*
		 * Reflect state change immediately to the user interface even
		 * if the wifi driver will take a while to perform the disconnect.
		 */
		_wifi_connection = Wifi_connection::disconnected_wifi_connection();

		_wlan_config.generate([&] (Xml_generator &xml) {

			xml.attribute("connected_scan_interval", 0U);
			xml.attribute("scan_interval", 10U);
			xml.attribute("use_11n", false);

			xml.attribute("verbose_state", false);
			xml.attribute("verbose",       false);

			xml.node("network", [&]() {
				/* generate attributes to ease subsequent manual tweaking */
				xml.attribute("ssid", "");
				xml.attribute("protection", "NONE");
				xml.attribute("passphrase", "");
			});
		});

		_runtime_config_generator.generate_runtime_config();
	}

	Network(Env &env, Allocator &alloc, Action &action,
	        Registry<Child_state> &child_states,
	        Runtime_config_generator &runtime_config_generator,
	        Runtime_info const &runtime_info, Pci_info const &pci_info)
	:
		_env(env), _alloc(alloc), _action(action), _child_states(child_states),
		_runtime_config_generator(runtime_config_generator),
		_runtime_info(runtime_info), _pci_info(pci_info)
	{
		/*
		 * Subscribe to reports
		 */
		_wlan_accesspoints_rom.sigh(_wlan_accesspoints_handler);
		_wlan_state_rom       .sigh(_wlan_state_handler);
		_nic_router_state_rom .sigh(_nic_router_state_handler);

		/*
		 * Evaluate and forward initial manually managed config
		 */
		_nic_router_config.with_manual_config([&] (Xml_node const &config) {
			_update_nic_target_from_config(config); });

		if (_nic_target.manual())
			_generate_nic_router_config();
	}
};

#endif /* _NETWORK_H_ */
