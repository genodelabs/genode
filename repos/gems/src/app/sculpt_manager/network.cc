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


void Sculpt::Network::_generate_nic_router_uplink(Xml_generator &xml,
                                                  char    const *label)
{
	xml.node("policy", [&] () {
		xml.attribute("label_prefix", label);
		xml.attribute("domain", "uplink");
	});
	gen_named_node(xml, "domain", "uplink", [&] () {
		xml.node("nat", [&] () {
			xml.attribute("domain",    "default");
			xml.attribute("tcp-ports", "1000");
			xml.attribute("udp-ports", "1000");
			xml.attribute("icmp-ids",  "1000");
		});
	});
}


void Sculpt::Network::handle_key_press(Codepoint code)
{
	enum { BACKSPACE = 8, ENTER = 10 };
	if (code.value == BACKSPACE)
		wpa_passphrase.remove_last_character();
	else if (code.value == ENTER) {
		if (wpa_passphrase.suitable_for_connect())
			wifi_connect(dialog.selected_ap());
	}
	else if (code.valid())
		wpa_passphrase.append_character(code);

	/*
	 * Keep updating the passphase when pressing keys after
	 * clicking the connect button once.
	 */
	if (_wifi_connection.state == Wifi_connection::CONNECTING)
		wifi_connect(_wifi_connection.bssid);

	_action.update_network_dialog();
}


void Sculpt::Network::_generate_nic_router_config()
{
	if (_nic_router_config.try_generate_manually_managed())
		return;

	if (!_nic_target.nic_router_needed()) {
		_nic_router_config.generate([&] (Xml_generator &xml) {
			xml.attribute("verbose_domain_state", "yes"); });
		return;
	}

	_nic_router_config.generate([&] (Xml_generator &xml) {
		xml.attribute("verbose_domain_state", "yes");

		xml.node("report", [&] () {
			xml.attribute("interval_sec",    "5");
			xml.attribute("bytes",           "yes");
			xml.attribute("config",          "yes");
			xml.attribute("config_triggers", "yes");
		});

		xml.node("default-policy", [&] () {
			xml.attribute("domain", "default"); });

		bool uplink_exists = true;
		switch (_nic_target.type()) {
		case Nic_target::WIRED: _generate_nic_router_uplink(xml, "nic_drv -> ");  break;
		case Nic_target::WIFI:  _generate_nic_router_uplink(xml, "wifi_drv -> "); break;
		case Nic_target::MODEM: _generate_nic_router_uplink(xml, "usb_net -> ");  break;
		default: uplink_exists = false;
		}
		gen_named_node(xml, "domain", "default", [&] () {
			xml.attribute("interface", "10.0.1.1/24");

			xml.node("dhcp-server", [&] () {
				xml.attribute("ip_first", "10.0.1.2");
				xml.attribute("ip_last",  "10.0.1.200");
				if (_nic_target.type() != Nic_target::DISCONNECTED) {
					xml.attribute("dns_config_from", "uplink"); }
			});

			if (uplink_exists) {
				xml.node("tcp", [&] () {
					xml.attribute("dst", "0.0.0.0/0");
					xml.node("permit-any", [&] () {
						xml.attribute("domain", "uplink"); }); });

				xml.node("udp", [&] () {
					xml.attribute("dst", "0.0.0.0/0");
					xml.node("permit-any", [&] () {
						xml.attribute("domain", "uplink"); }); });

				xml.node("icmp", [&] () {
					xml.attribute("dst", "0.0.0.0/0");
					xml.attribute("domain", "uplink"); });
			}
		});
	});
}


void Sculpt::Network::_handle_wlan_accesspoints()
{
	bool const initial_scan = !_wlan_accesspoints_rom.xml().has_sub_node("accesspoint");

	_wlan_accesspoints_rom.update();

	/* suppress updating the list while the access-point list is hovered */
	if (!initial_scan && dialog.ap_list_hovered())
		return;

	_access_points.update_from_xml(_wlan_accesspoints_rom.xml(),

		/* create */
		[&] (Xml_node const &node) -> Access_point &
		{
			auto const protection = node.attribute_value("protection", String<16>());
			bool const use_protection = protection == "WPA"  ||
			                            protection == "WPA2" ||
			                            protection == "WPA3";

			return *new (_alloc)
				Access_point(node.attribute_value("bssid", Access_point::Bssid()),
				             node.attribute_value("ssid",  Access_point::Ssid()),
				             use_protection ? Access_point::Protection::WPA_PSK
				                            : Access_point::Protection::UNPROTECTED);
		},

		/* destroy */
		[&] (Access_point &ap) { destroy(_alloc, &ap); },

		/* update */
		[&] (Access_point &ap, Xml_node const &node)
		{
			ap.quality = node.attribute_value("quality", 0U);
		}
	);

	_action.update_network_dialog();
}


void Sculpt::Network::_handle_wlan_state()
{
	_wlan_state_rom.update();
	_wifi_connection = Wifi_connection::from_xml(_wlan_state_rom.xml());
	_action.update_network_dialog();
}


void Sculpt::Network::_handle_nic_router_state()
{
	_nic_router_state_rom.update();

	Nic_state const old_nic_state = _nic_state;
	_nic_state = Nic_state::from_xml(_nic_router_state_rom.xml());

	if (_nic_state.ipv4 != old_nic_state.ipv4)
		_action.update_network_dialog();

	/* if the nic state becomes ready, consider spawning the update subsystem */
	if (old_nic_state.ready() != _nic_state.ready())
		_runtime_config_generator.generate_runtime_config();
}


void Sculpt::Network::_update_nic_target_from_config(Xml_node const &config)
{
	_nic_target.policy = config.has_type("empty")
	                   ? Nic_target::MANAGED : Nic_target::MANUAL;

	/* obtain uplink information from configuration */
	auto nic_target_from_config = [] (Xml_node const &config)
	{
		if (!config.has_sub_node("domain"))
			return Nic_target::OFF;

		Nic_target::Type result = Nic_target::DISCONNECTED;

		config.for_each_sub_node("policy", [&] (Xml_node uplink) {

			/* skip uplinks not assigned to a domain called "uplink" */
			if (uplink.attribute_value("domain", String<16>()) != "uplink")
				return;

			if (uplink.attribute_value("label_prefix", String<16>()) == "nic_drv -> ")
				result = Nic_target::WIRED;

			if (uplink.attribute_value("label_prefix", String<16>()) == "wifi_drv -> ")
				result = Nic_target::WIFI;

			if (uplink.attribute_value("label_prefix", String<16>()) == "usb_net -> ")
				result = Nic_target::MODEM;
		});
		return result;
	};

	if (_nic_target.manual())
		_nic_target.manual_type = nic_target_from_config(config);
}



void Sculpt::Network::_handle_nic_router_config(Xml_node config)
{
	_update_nic_target_from_config(config);
	_generate_nic_router_config();
	_runtime_config_generator.generate_runtime_config();
	_action.update_network_dialog();
}


void Sculpt::Network::gen_runtime_start_nodes(Xml_generator &xml) const
{
	switch (_nic_target.type()) {
	case Nic_target::WIRED:

		xml.node("start", [&] () {
			xml.attribute("version", _nic_drv_version);
			gen_nic_drv_start_content(xml);
		});
		xml.node("start", [&] () { gen_nic_router_start_content(xml); });
		break;

	case Nic_target::WIFI:

		xml.node("start", [&] () {
			xml.attribute("version", _wifi_drv_version);
			gen_wifi_drv_start_content(xml);
		});
		xml.node("start", [&] () { gen_nic_router_start_content(xml); });
		break;

	case Nic_target::MODEM:

		xml.node("start", [&] () {
			xml.attribute("version", _usb_net_version);
			gen_usb_net_start_content(xml);
		});
		xml.node("start", [&] () { gen_nic_router_start_content(xml); });
		break;

	case Nic_target::DISCONNECTED:

		xml.node("start", [&] () { gen_nic_router_start_content(xml); });
		break;

	case Nic_target::OFF:
		break;

	case Nic_target::UNDEFINED:
		break;
	}
}
