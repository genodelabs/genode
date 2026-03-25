/*
 * \brief  Network management widget
 * \author Norman Feske
 * \date   2018-05-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__NETWORK_WIDGET_H_
#define _VIEW__NETWORK_WIDGET_H_

/* local includes */
#include <model/nic_state.h>
#include <model/board_info.h>
#include <model/runtime_state.h>
#include <view/ap_selector_widget.h>

namespace Sculpt { struct Network_widget; }


struct Sculpt::Network_widget : Widget<Frame>
{
	using Wlan_config_policy = Ap_selector_widget::Wlan_config_policy;

	Nic_state const &_nic_state;

	enum class Target { DISCONNECTED, NIC, WIFI, MOBILE };

	struct Action : Ap_selector_widget::Action
	{
		virtual void nic_target(Target) = 0;
	};

	struct View_attr
	{
		struct {
			bool nic, wifi, mobile;
			bool any() const { return nic || wifi || mobile; }

		} enabled;

		static View_attr from_runtime(Runtime_state const &runtime)
		{
			return { .enabled = { .nic    = runtime.present_in_runtime("nic"),
			                      .wifi   = runtime.present_in_runtime("wifi"),
			                      .mobile = runtime.present_in_runtime("mobile") } };
		}
	};

	struct Target_selector : Widget<Hbox>
	{
		Hosted<Hbox, Toggle_button>
			_local  { Id { "Disconected" } },
			_nic    { Id { "Wired"       } },
			_wifi   { Id { "Wifi"        } },
			_mobile { Id { "Mobile data" } };

		void view(Scope<Hbox> &s, View_attr const attr, Board_info const &board_info) const
		{
			s.widget(_local, !attr.enabled.any());

			if (board_info.detected.nic || board_info.soc.nic)
				s.widget(_nic, attr.enabled.nic);

			if (board_info.wifi_avail())
				s.widget(_wifi, attr.enabled.wifi);

			if (board_info.soc.modem)
				s.widget(_mobile, attr.enabled.mobile);
		}

		void click(Clicked_at const &at, Action &action)
		{
			_local .propagate(at, [&] { action.nic_target(Target::DISCONNECTED); });
			_nic   .propagate(at, [&] { action.nic_target(Target::NIC); });
			_wifi  .propagate(at, [&] { action.nic_target(Target::WIFI); });
			_mobile.propagate(at, [&] { action.nic_target(Target::MOBILE); });
		}
	};

	Hosted<Frame, Vbox, Target_selector> _target_selector { Id { "target" } };

	Hosted<Frame, Vbox, Frame, Vbox, Ap_selector_widget> _ap_selector;

	void _gen_connected_ap(Generator &, bool) const;

	Network_widget(Access_points        const &access_points,
	               Wifi_connection      const &wifi_connection,
	               Nic_state            const &nic_state,
	               Blind_wpa_passphrase const &wpa_passphrase,
	               Wlan_config_policy   const &wlan_config_policy)
	:
		_nic_state(nic_state),
		_ap_selector(Id { "aps" },
		             access_points, wifi_connection,
		             wlan_config_policy, wpa_passphrase)
	{ }

	void view(Scope<Frame> &s, Board_info const &board_info, View_attr attr) const
	{
		s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
			s.sub_scope<Min_ex>(35);

			s.widget(_target_selector, attr, board_info);

			if (attr.enabled.any()) {
				s.sub_scope<Frame>([&] (Scope<Frame, Vbox, Frame> &s) {
					s.sub_scope<Vbox>([&] (Scope<Frame, Vbox, Frame, Vbox> &s) {

						if (attr.enabled.wifi)
							s.widget(_ap_selector);

						if (_nic_state.ready())
							s.sub_scope<Label>(_nic_state.ipv4);
					});
				});
			}
		});
	}

	void click(Clicked_at const &at, Action &action)
	{
		_target_selector.propagate(at, action);
		_ap_selector.propagate(at, action);
	}

	bool need_keyboard_focus_for_passphrase() const
	{
		return _ap_selector.need_keyboard_focus_for_passphrase();
	}

	bool ap_list_hovered(Hovered_at const &at) const
	{
		return _ap_selector.if_hovered(at, [&] (Hovered_at const &) {
			return _ap_selector.ap_list_shown(); });
	}
};

#endif /* _VIEW__NETWORK_WIDGET_H_ */
