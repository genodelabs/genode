/*
 * \brief  Network management dialog
 * \author Norman Feske
 * \date   2018-05-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* local includes */
#include "network_dialog.h"

using namespace Sculpt;

void Network_dialog::_gen_access_point(Xml_generator &xml,
                                       Access_point const &ap) const
{
	gen_named_node(xml, "hbox", ap.bssid, [&] () {

		gen_named_node(xml, "float", "left", [&] () {
			xml.attribute("west", "yes");

			xml.node("hbox", [&] () {
				gen_named_node(xml, "button", "button", [&] () {
					xml.attribute("style", "radio");

					if (_wifi_connection.connected())
						xml.attribute("selected", "yes");
					else
						_ap_item.gen_button_attr(xml, ap.bssid);

					xml.node("hbox", [&] () { });
				});

				gen_named_node(xml, "label", "ssid", [&] () {
					xml.attribute("text", String<20>(" ", ap.ssid)); });

				gen_named_node(xml, "label", "protection", [&] () {
					xml.attribute("font", "annotation/regular");
					if (ap.protection == Access_point::WPA_PSK)
						xml.attribute("text", " (WPA) ");
					else
						xml.attribute("text", " ");
				});
			});
		});

		gen_named_node(xml, "float", "right", [&] () {
			xml.attribute("east", "yes");
			xml.node("label", [&] () {
				xml.attribute("text", String<8>(ap.quality, "%")); });
		});
	});
}


bool Network_dialog::_selected_ap_visible() const
{
	unsigned cnt = 0;
	return _for_each_ap([&] (Access_point const &ap) {
		return (cnt++ < _max_visible_aps) && _ap_item.selected(ap.bssid); });
}


bool Network_dialog::_selected_ap_unprotected() const
{
	return _for_each_ap([&] (Access_point const &ap) {
		return _ap_item.selected(ap.bssid) && ap.unprotected(); });
}


bool Network_dialog::need_keyboard_focus_for_passphrase() const
{
	if (   _wifi_connection.state == Wifi_connection::CONNECTED
	    || _wifi_connection.state == Wifi_connection::CONNECTING)
		return false;

	if (!_nic_target.wifi())
		return false;

	return _for_each_ap([&] (Access_point const &ap) {
		return _ap_item.selected(ap.bssid) && ap.wpa_protected(); });
}


void Network_dialog::_gen_access_point_list(Xml_generator &xml,
                                            bool auth_failure) const
{
	if (_wlan_config_policy == WLAN_CONFIG_MANUAL)
		return;

	bool const selected_ap_visible = _selected_ap_visible();

	unsigned cnt = 0;
	_access_points.for_each([&] (Access_point const &ap) {

		if (cnt++ >= _max_visible_aps)
			return;

		/*
		 * Whenever the user has selected an access point, hide all others.
		 * Should the selected AP disappear from the list, show all others.
		 */
		bool const selected = _ap_item.selected(ap.bssid);
		if (selected_ap_visible && !selected)
			return;

		_gen_access_point(xml, ap);

		if (!selected)
			return;

		bool const connected_to_selected_ap =
			(selected && _wifi_connection.bssid == ap.bssid)
			&& _wifi_connection.state == Wifi_connection::CONNECTED;

		if (connected_to_selected_ap)
			return;

		if (ap.protection == Access_point::WPA_PSK) {
			gen_named_node(xml, "label", "passphrase msg", [&] () {
				xml.attribute("text", auth_failure ? "Enter passphrase (auth failure):"
				                                   : "Enter passphrase:");
			});

			gen_named_node(xml, "frame", "passphrase", [&] () {
				xml.node("float", [&] () {
					xml.attribute("west", "yes");
					xml.node("label", [&] () {
						xml.attribute("font", "title/regular");
						String<3*64> const passphrase(" ", _wpa_passphrase);
						xml.attribute("text", passphrase);
						xml.node("cursor", [&] () {
							xml.attribute("at", passphrase.length() - 1);
						});
					});
				});
			});

			if (_wpa_passphrase.suitable_for_connect()) {
				xml.node("button", [&] () {

					if (_wifi_connection.state == Wifi_connection::CONNECTING)
						xml.attribute("selected", "yes");

					/* suppress hover while connecting */
					else
						_connect_item.gen_button_attr(xml, "connect");

					xml.node("label", [&] () {
						xml.attribute("text", "Connect"); });
				});
			}
		}
	});

	/*
	 * Present motivational message until we get the first 'accesspoints'
	 * report.
	 */
	if (cnt == 0)
		xml.node("label", [&] () {
			xml.attribute("text", "Scanning..."); });
}


void Network_dialog::_gen_connected_ap(Xml_generator &xml, bool connected) const
{
	bool done = false;

	/*
	 * Try to present complete info including the quality from access-point
	 * list.
	 */
	_access_points.for_each([&] (Access_point const &ap) {
		if (!done && _wifi_connection.bssid == ap.bssid) {
			_gen_access_point(xml, ap);
			done = true;
		}
	});

	/*
	 * If access point is not present in the list, fall back to the information
	 * given in the 'state' report.
	 */
	if (!done)
		_gen_access_point(xml, Access_point { _wifi_connection.bssid,
		                                      _wifi_connection.ssid,
		                                      Access_point::UNKNOWN });

	gen_named_node(xml, "label", "associated", [&] () {
		xml.attribute("text", connected ? "associated"
		                                : "connecting");
	});
}


void Network_dialog::generate(Xml_generator &xml) const
{
	gen_named_node(xml, "frame", "network", [&] () {
		xml.node("vbox", [&] () {

			gen_named_node(xml, "hbox", "type", [&] () {

				auto gen_nic_button = [&] (Hoverable_item::Id const &id,
				                           Nic_target::Type   const  type,
				                           String<20>         const &label) {
					gen_named_node(xml, "button", id, [&] () {

						_nic_item.gen_button_attr(xml, id);

						if (_nic_target.type() == type)
							xml.attribute("selected", "yes");

						xml.node("label", [&] () { xml.attribute("text", label); });
					});
				};

				gen_nic_button("off", Nic_target::OFF, "Off");

				/*
				 * Allow interactive selection only if NIC-router configuration
				 * is not manually maintained.
				 */
				if (_nic_target.managed() || _nic_target.manual_type == Nic_target::DISCONNECTED)
					gen_nic_button("disconnected", Nic_target::DISCONNECTED, "Disconnected");

				if (_nic_target.managed() || _nic_target.manual_type == Nic_target::WIRED)
					if (_pci_info.lan_present)
						gen_nic_button("wired", Nic_target::WIRED, "Wired");

				if (_nic_target.managed() || _nic_target.manual_type == Nic_target::WIFI)
					if (_pci_info.wifi_present)
						gen_nic_button("wifi",  Nic_target::WIFI,  "Wifi");

				if (_nic_target.managed() || _nic_target.manual_type == Nic_target::MODEM)
					if (_pci_info.modem_present)
						gen_nic_button("modem",  Nic_target::MODEM,  "Mobile data");
			});

			if (_nic_target.wifi() || _nic_target.wired() || _nic_target.modem()) {
				gen_named_node(xml, "frame", "nic_info", [&] () {
					xml.node("vbox", [&] () {

						/*
						 * If connected via Wifi, show the information of the
						 * connected access point. If not connected, present
						 * the complete list of access points with the option
						 * to select one.
						 */
						if (_nic_target.wifi()) {
							if (_wifi_connection.connected())
								_gen_connected_ap(xml, true);
							else if (_wifi_connection.connecting())
								_gen_connected_ap(xml, false);
							else
								_gen_access_point_list(xml,
								                       _wifi_connection.auth_failure());
						}

						/* append display of uplink IP address */
						if (_nic_state.ready())
							gen_named_node(xml, "label", "ip", [&] () {
								xml.attribute("text", _nic_state.ipv4); });
					});
				});
			}
		});
	});
}


Dialog::Hover_result Network_dialog::hover(Xml_node hover)
{
	Dialog::Hover_result const hover_result = Dialog::any_hover_changed(
		_nic_item    .match(hover, "frame", "vbox", "hbox", "button", "name"),
		_ap_item     .match(hover, "frame", "vbox", "frame", "vbox", "hbox", "name"),
		_connect_item.match(hover, "frame", "vbox", "frame", "vbox", "button", "name"));

	_nic_info.match(hover, "vbox", "frame", "name");

	return hover_result;
}


void Network_dialog::click(Action &action)
{
	if (_nic_item.hovered("off"))          action.nic_target(Nic_target::OFF);
	if (_nic_item.hovered("disconnected")) action.nic_target(Nic_target::DISCONNECTED);
	if (_nic_item.hovered("wired"))        action.nic_target(Nic_target::WIRED);
	if (_nic_item.hovered("wifi"))         action.nic_target(Nic_target::WIFI);
	if (_nic_item.hovered("modem"))        action.nic_target(Nic_target::MODEM);

	if (_wifi_connection.connected() && _ap_item.hovered(_wifi_connection.bssid)) {
		action.wifi_disconnect();
		_ap_item.reset();
	} else {
		_ap_item.toggle_selection_on_click();

		/* immediately connect to unprotected access point when selected */
		if (_ap_item.any_selected() && _selected_ap_unprotected())
			action.wifi_connect(selected_ap());
	}

	if (_connect_item.hovered("connect"))
		action.wifi_connect(selected_ap());
}

