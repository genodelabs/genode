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
#include <model/board_info.h>
#include <view/network_widget.h>
#include <runtime.h>
#include <keyboard_focus.h>
#include <managed_config.h>

namespace Sculpt { struct Network; }


struct Sculpt::Network : Noncopyable
{
	Env &_env;

	Allocator &_alloc;

	struct Action : Interface
	{
		virtual void network_config_changed() = 0;
	};

	struct Info : Interface
	{
		virtual bool ap_list_hovered() const = 0;
	};

	Action     &_action;
	Info const &_info;

	Registry<Child_state> &_child_states;

	Runtime_config_generator &_runtime_config_generator;

	using Wlan_config_policy = Network_widget::Wlan_config_policy;

	Nic_target _nic_target { };
	Nic_state  _nic_state  { };

	Access_point::Bssid _selected_ap { };

	Wpa_passphrase wpa_passphrase { };

	Rom_handler<Network> _wlan_accesspoints_rom {
		_env, "report -> runtime/wifi/accesspoints", *this, &Network::_handle_wlan_accesspoints };

	Rom_handler<Network> _wlan_state_rom {
		_env, "report -> runtime/wifi/state", *this, &Network::_handle_wlan_state };

	Rom_handler<Network> _nic_router_state_rom {
		_env, "report -> runtime/nic_router/state", *this, &Network::_handle_nic_router_state };

	void _generate_nic_router_config();

	void _generate_nic_router_uplink(Xml_generator &xml, char const *label);

	Access_points _access_points { };

	Wifi_connection _wifi_connection = Wifi_connection::disconnected_wifi_connection();

	void gen_runtime_start_nodes(Xml_generator &xml) const;

	bool ready() const { return _nic_target.ready() && _nic_state.ready(); }

	void handle_key_press(Codepoint);

	void _handle_wlan_accesspoints(Xml_node const &);
	void _handle_wlan_state(Xml_node const &);
	void _handle_nic_router_state(Xml_node const &);
	void _handle_nic_router_config(Xml_node const &);

	Managed_config<Network> _nic_router_config {
		_env, "config", "nic_router", *this, &Network::_handle_nic_router_config };

	Wlan_config_policy _wlan_config_policy = Wlan_config_policy::MANAGED;

	Network_widget dialog {
		_nic_target, _access_points,
		_wifi_connection, _nic_state, wpa_passphrase, _wlan_config_policy };

	Managed_config<Network> _wlan_config {
		_env, "config", "wifi", *this, &Network::_handle_wlan_config };

	void _handle_wlan_config(Xml_node const &)
	{
		if (_wlan_config.try_generate_manually_managed()) {
			_wlan_config_policy = Wlan_config_policy::MANUAL;
			_action.network_config_changed();
			return;
		}

		_wlan_config_policy = Wlan_config_policy::MANAGED;;

		if (_wifi_connection.connected())
			wifi_connect(_wifi_connection.bssid);
		else
			wifi_disconnect();
	}

	void _update_nic_target_from_config(Xml_node const &);

	void nic_target(Nic_target::Type const type)
	{
		if (type != _nic_target.managed_type) {
			_nic_target.managed_type = type;
			_generate_nic_router_config();
			_runtime_config_generator.generate_runtime_config();
			_action.network_config_changed();
		}
	}

	void wifi_connect(Access_point::Bssid bssid)
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
				xml.attribute("update_quality_interval", 30U);
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

	void wifi_disconnect()
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

	Network(Env &env, Allocator &alloc, Action &action, Info const &info,
	        Registry<Child_state> &child_states,
	        Runtime_config_generator &runtime_config_generator)
	:
		_env(env), _alloc(alloc), _action(action), _info(info),
		_child_states(child_states),
		_runtime_config_generator(runtime_config_generator)
	{
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
