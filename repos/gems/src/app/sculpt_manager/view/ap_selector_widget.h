/*
 * \brief  Access-point selector
 * \author Norman Feske
 * \date   2023-11-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__AP_SELECTOR_WIDGET_H_
#define _VIEW__AP_SELECTOR_WIDGET_H_

/* local includes */
#include <types.h>
#include <model/wifi_connection.h>
#include <model/wpa_passphrase.h>
#include <view/dialog.h>

namespace Sculpt { struct Ap_selector_widget; }


struct Sculpt::Ap_selector_widget : Widget<Vbox>
{
	enum class Wlan_config_policy { MANAGED, MANUAL };

	Access_points        const &_access_points;
	Wifi_connection      const &_wifi_connection;
	Wlan_config_policy   const &_wlan_config_policy;
	Blind_wpa_passphrase const &_wpa_passphrase;

	struct Action : Interface
	{
		virtual void wifi_connect(Access_point::Ssid) = 0;
		virtual void wifi_disconnect() = 0;
	};

	/* limit view to highest-quality access points */
	unsigned const _max_visible_aps = 20;

	Access_point::Bssid _selected { };

	Hosted<Vbox, Action_button> _connect { Id { "Connect" } };

	struct Item : Widget<Hbox>
	{
		struct Attr { bool selected; };

		void view(Scope<Hbox> &s, Access_point const &ap, Attr attr) const
		{
			bool const hovered = s.hovered();

			s.sub_scope<Left_floating_hbox>([&] (Scope<Hbox, Left_floating_hbox> &s) {
				s.sub_scope<Icon>("radio", Icon::Attr { .hovered  = hovered,
				                                        .selected = attr.selected });
				s.sub_scope<Label>(String<20>(" ", ap.ssid));
				s.sub_scope<Annotation>((ap.protection == Access_point::WPA_PSK)
				                        ? " (WPA) " : " ");
			});

			s.sub_scope<Float>([&] (Scope<Hbox, Float> &s) {
				s.attribute("east", "yes");
				s.sub_scope<Label>(String<8>(ap.quality, "%"));
			});
		}
	};

	/*
	 * \return true if at least one access point fulfils the condition 'cond_fn'
	 */
	bool _for_each_ap(auto const &cond_fn) const
	{
		bool result = false;
		_access_points.for_each([&] (Access_point const &ap) {
			result |= cond_fn(ap); });
		return result;
	}

	bool _selected_ap_visible() const
	{
		unsigned count = 0;
		return _for_each_ap([&] (Access_point const &ap) {
			return (count++ < _max_visible_aps) && (_selected == ap.bssid); });
	}

	void _with_selected_ap(auto const &fn) const
	{
		bool done = false;
		_access_points.for_each([&] (Access_point const &ap) {
			if (!done && (ap.bssid == _selected)) {
				fn(ap);
				done = true; } });

		/*
		 * If access point is not present in the list, fall back to the information
		 * given in the 'state' report.
		 */
		if (!done)
			fn(Access_point { _wifi_connection.bssid,
			                  _wifi_connection.ssid,
			                  Access_point::UNKNOWN });
	}

	Ap_selector_widget(Access_points        const &aps,
	                   Wifi_connection      const &wifi_connection,
	                   Wlan_config_policy   const &wlan_config_policy,
	                   Blind_wpa_passphrase const &wpa_passphrase)
	:
		_access_points(aps), _wifi_connection(wifi_connection),
		_wlan_config_policy(wlan_config_policy), _wpa_passphrase(wpa_passphrase)
	{ }

	void view(Scope<Vbox> &s) const
	{
		if (_wlan_config_policy == Wlan_config_policy::MANUAL)
			return;

		if (_wifi_connection.connecting() || _wifi_connection.connected()) {

			Hosted<Vbox, Item> item { Id { _selected } };

			_with_selected_ap([&] (Access_point const &ap) {
				s.widget(item, ap, Item::Attr { .selected = true }); });

			s.sub_scope<Label>(_wifi_connection.connecting()
			                   ? "connecting" : "associated" );
			return;
		}

		bool const selected_ap_visible = _selected_ap_visible();

		unsigned count = 0;
		_access_points.for_each([&] (Access_point const &ap) {

			if (count++ >= _max_visible_aps)
				return;

			/*
			 * Whenever the user has selected an access point, hide all others.
			 * Should the selected AP disappear from the list, show all others.
			 */
			bool const selected = (_selected == ap.bssid);
			if (selected_ap_visible && !selected)
				return;

			Hosted<Vbox, Item> item { Id { ap.bssid } };
			s.widget(item, ap, Item::Attr { .selected = selected });

			if (!selected)
				return;

			bool const connected_to_selected_ap =
				(selected && _wifi_connection.ssid == ap.ssid)
				&& _wifi_connection.state == Wifi_connection::CONNECTED;

			if (connected_to_selected_ap)
				return;

			if (ap.protection == Access_point::WPA_PSK) {
				s.sub_scope<Label>(_wifi_connection.auth_failure()
				                   ? "Enter passphrase (auth failure):"
				                   : "Enter passphrase:");

				s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
					s.sub_scope<Float>([&] (Scope<Vbox, Frame, Float> &s) {
						s.attribute("west", "yes");
						String<3*64> const passphrase(" ", _wpa_passphrase);
						s.sub_scope<Label>(passphrase, [&] (auto &s) {
							s.attribute("font", "title/regular");
							s.sub_node("cursor", [&] {
								s.attribute("at", passphrase.length() - 1); });
						});
					});
				});

				if (_wpa_passphrase.suitable_for_connect())
					s.widget(_connect);
			}
		});

		/*
		 * Present motivational message until we get the first 'accesspoints'
		 * report.
		 */
		if (count == 0)
			s.sub_scope<Label>("Scanning...");
	}

	bool need_keyboard_focus_for_passphrase() const
	{
		if (_wifi_connection.state == Wifi_connection::CONNECTED
		 || _wifi_connection.state == Wifi_connection::CONNECTING)
			return false;

		return _for_each_ap([&] (Access_point const &ap) {
			return (_selected == ap.bssid) && ap.wpa_protected(); });
	}

	void click(Clicked_at const &at, Action &action)
	{
		Id const ap_id = at.matching_id<Vbox, Item>();

		if (ap_id.valid()) {

			if (ap_id.value == _selected) {
				action.wifi_disconnect();
				_selected = { };

			} else {
				_selected = ap_id.value;

				auto selected_ap_unprotected = [&] {
					return _for_each_ap([&] (Access_point const &ap) {
						return (_selected == ap.bssid) && ap.unprotected(); }); };

				/* immediately connect to unprotected access point when selected */
				if (selected_ap_unprotected())
					action.wifi_connect(_selected);
			}
		}

		_connect.propagate(at, [&] { action.wifi_connect(_selected); });
	}

	bool ap_list_shown() const { return !_selected.valid(); }
};

#endif /* _VIEW__AP_SELECTOR_WIDGET_H_ */
