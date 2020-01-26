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
	xml.node("uplink", [&] () {
		xml.attribute("label", label);
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
	else if (code.value == ENTER)
		wifi_connect(dialog.selected_ap());
	else if (code.valid())
		wpa_passphrase.append_character(code);

	/*
	 * Keep updating the passphase when pressing keys after
	 * clicking the connect button once.
	 */
	if (_wifi_connection.state == Wifi_connection::CONNECTING)
		wifi_connect(_wifi_connection.bssid);

	_menu_view.generate();
}


void Sculpt::Network::_generate_nic_router_config()
{
	if ((_nic_target.wired() && !_runtime_info.present_in_runtime("nic_drv"))
	 || (_nic_target.wifi()  && !_runtime_info.present_in_runtime("wifi_drv"))) {

		/* defer NIC router reconfiguration until the needed uplink is present */
		_nic_router_config_up_to_date = false;
		return;
	}

	_nic_router_config_up_to_date = true;

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
		case Nic_target::WIRED: _generate_nic_router_uplink(xml, "wired"); break;
		case Nic_target::WIFI:  _generate_nic_router_uplink(xml, "wifi");  break;
		default: uplink_exists = false;
		}
		gen_named_node(xml, "domain", "default", [&] () {
			xml.attribute("interface", "10.0.1.1/24");

			xml.node("dhcp-server", [&] () {
				xml.attribute("ip_first", "10.0.1.2");
				xml.attribute("ip_last",  "10.0.1.200");
				if (_nic_target.type() != Nic_target::LOCAL) {
					xml.attribute("dns_server_from", "uplink"); }
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

	Access_point_update_policy policy(_alloc);
	_access_points.update_from_xml(policy, _wlan_accesspoints_rom.xml());
	_menu_view.generate();
}


void Sculpt::Network::_handle_wlan_state()
{
	_wlan_state_rom.update();
	_wifi_connection = Wifi_connection::from_xml(_wlan_state_rom.xml());
	_menu_view.generate();
}


void Sculpt::Network::_handle_nic_router_state()
{
	_nic_router_state_rom.update();

	Nic_state const old_nic_state = _nic_state;
	_nic_state = Nic_state::from_xml(_nic_router_state_rom.xml());

	if (_nic_state.ipv4 != old_nic_state.ipv4)
		_menu_view.generate();

	/* if the nic state becomes ready, consider spawning the update subsystem */
	if (old_nic_state.ready() != _nic_state.ready())
		_runtime_config_generator.generate_runtime_config();
}


void Sculpt::Network::_handle_nic_router_config(Xml_node config)
{
	Nic_target::Type target = _nic_target.managed_type;

	_nic_target.policy = config.has_type("empty")
	                   ? Nic_target::MANAGED : Nic_target::MANUAL;

	if (_nic_target.manual()) {

		/* obtain uplink information from configuration */
		target = Nic_target::LOCAL;

		if (!config.has_sub_node("domain"))
			target = Nic_target::OFF;

		struct Break : Exception { };
		try {
			config.for_each_sub_node("domain", [&] (Xml_node domain) {

				/* skip domains that are not called "uplink" */
				if (domain.attribute_value("name", String<16>()) != "uplink")
					return;

				config.for_each_sub_node("uplink", [&] (Xml_node uplink) {

					/* skip uplinks not assigned to a domain called "uplink" */
					if (uplink.attribute_value("domain", String<16>()) != "uplink")
						return;

					if (uplink.attribute_value("label", String<16>()) == "wired") {
						target = Nic_target::WIRED;
						throw Break();
					}
					if (uplink.attribute_value("label", String<16>()) == "wifi") {
						target = Nic_target::WIFI;
						throw Break();
					}
				});
			});
		} catch (Break) { }
		_nic_target.manual_type = target;
	}

	nic_target(target);
	_generate_nic_router_config();
	_runtime_config_generator.generate_runtime_config();
	_menu_view.generate();
}


void Sculpt::Network::gen_runtime_start_nodes(Xml_generator &xml) const
{
	if (_use_nic_drv)
		xml.node("start", [&] () { gen_nic_drv_start_content(xml); });

	if (_use_wifi_drv)
		xml.node("start", [&] () { gen_wifi_drv_start_content(xml); });

	if (_nic_target.type() != Nic_target::OFF)
		xml.node("start", [&] () {
			gen_nic_router_start_content(xml, _nic_target,
			                             _use_nic_drv, _use_wifi_drv); });
}
