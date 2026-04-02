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

/* local includes */
#include <network.h>


void Sculpt::Network::handle_key_press(Codepoint code)
{
	enum { BACKSPACE = 8, ENTER = 10 };
	if (code.value == BACKSPACE)
		wpa_passphrase.remove_last_character();
	else if (code.value == ENTER) {
		if (wpa_passphrase.suitable_for_connect())
			wifi_connect(dialog._ap_selector._selected);
	}
	else if (code.valid())
		wpa_passphrase.append_character(code);

	/*
	 * Keep updating the passphase when pressing keys after
	 * clicking the connect button once.
	 */
	if (_wifi_connection.state == Wifi_connection::CONNECTING)
		wifi_connect(_wifi_connection.bssid);

	_action.network_config_changed();
}


void Sculpt::Network::_handle_wlan_accesspoints(Node const &accesspoints)
{
	bool const initial_scan = !accesspoints.has_sub_node("accesspoint");

	/* suppress updating the list while the access-point list is hovered */
	if (!initial_scan && _info.ap_list_hovered())
		return;

	auto protection_from_node = [&] (Node const &node) {
		auto const protection = node.attribute_value("protection", String<16>());

		if (protection == "WPA" || protection == "WPA2")
			return Access_point::Protection::WPA_PSK;

		if (protection == "WPA3")
			return Access_point::Protection::WPA3_PSK;

		return Access_point::Protection::UNPROTECTED;
	};

	_access_points.update_from_node(accesspoints,

		/* create */
		[&] (Node const &node) -> Access_point &
		{
			return *new (_alloc)
				Access_point(node.attribute_value("bssid", Access_point::Bssid()),
				             node.attribute_value("ssid",  Access_point::Ssid()),
				             protection_from_node(node));
		},

		/* destroy */
		[&] (Access_point &ap) { destroy(_alloc, &ap); },

		/* update */
		[&] (Access_point &ap, Node const &node)
		{
			ap.quality = node.attribute_value("quality", 0U);
		}
	);

	_action.network_config_changed();
}


void Sculpt::Network::_handle_wlan_state(Node const &state)
{
	_wifi_connection = Wifi_connection::from_node(state);
	_action.network_config_changed();
}


void Sculpt::Network::_handle_nic_router_state(Node const &state)
{
	Nic_state const orig = _nic_state;
	_nic_state = Nic_state::from_node(state);

	/* if the nic state becomes ready, consider spawning the update subsystem */
	if (_nic_state.ipv4 != orig.ipv4 || orig.ready() != _nic_state.ready())
		_action.network_config_changed();
}
