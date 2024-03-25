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
#include <model/nic_target.h>
#include <model/nic_state.h>
#include <model/board_info.h>
#include <view/ap_selector_widget.h>

namespace Sculpt { struct Network_widget; }


struct Sculpt::Network_widget : Widget<Frame>
{
	using Wlan_config_policy = Ap_selector_widget::Wlan_config_policy;

	Nic_target const &_nic_target;
	Nic_state  const &_nic_state;

	struct Action : Ap_selector_widget::Action
	{
		virtual void nic_target(Nic_target::Type) = 0;
	};

	struct Target_selector : Widget<Hbox>
	{
		using Type = Nic_target::Type;

		Hosted<Hbox, Select_button<Type>>
			_off   { Id { "Off"          }, Type::OFF          },
			_local { Id { "Disconnected" }, Type::DISCONNECTED },
			_wired { Id { "Wired"        }, Type::WIRED        },
			_wifi  { Id { "Wifi"         }, Type::WIFI         },
			_modem { Id { "Mobile data"  }, Type::MODEM        };

		void view(Scope<Hbox> &s, Nic_target const &target, Board_info const &board_info) const
		{
			Type const selected = target.type();

			s.widget(_off, selected);

			/*
			 * Allow interactive selection only if NIC-router configuration
			 * is not manually maintained.
			 */
			if (target.managed() || target.manual_type == Nic_target::DISCONNECTED)
				s.widget(_local, selected);

			if (target.managed() || target.manual_type == Nic_target::WIRED)
				if (board_info.detected.nic)
					s.widget(_wired, selected);

			if (target.managed() || target.manual_type == Nic_target::WIFI)
				if (board_info.wifi_avail())
					s.widget(_wifi, selected);

			if (target.managed() || target.manual_type == Nic_target::MODEM)
				if (board_info.soc.modem)
					s.widget(_modem, selected);
		}

		void click(Clicked_at const &at, Action &action)
		{
			_off  .propagate(at, [&] (Type t) { action.nic_target(t); });
			_local.propagate(at, [&] (Type t) { action.nic_target(t); });
			_wired.propagate(at, [&] (Type t) { action.nic_target(t); });
			_wifi .propagate(at, [&] (Type t) { action.nic_target(t); });
			_modem.propagate(at, [&] (Type t) { action.nic_target(t); });
		}
	};

	Hosted<Frame, Vbox, Target_selector> _target_selector { Id { "target" } };

	Hosted<Frame, Vbox, Frame, Vbox, Ap_selector_widget> _ap_selector;

	void _gen_connected_ap(Xml_generator &, bool) const;

	Network_widget(Nic_target           const &nic_target,
	               Access_points        const &access_points,
	               Wifi_connection      const &wifi_connection,
	               Nic_state            const &nic_state,
	               Blind_wpa_passphrase const &wpa_passphrase,
	               Wlan_config_policy   const &wlan_config_policy)
	:
		_nic_target(nic_target), _nic_state(nic_state),
		_ap_selector(Id { "aps" },
		             access_points, wifi_connection,
		             wlan_config_policy, wpa_passphrase)
	{ }

	void view(Scope<Frame> &s, Board_info const &board_info) const
	{
		s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
			s.sub_scope<Min_ex>(35);

			s.widget(_target_selector, _nic_target, board_info);

			if (_nic_target.wifi() || _nic_target.wired() || _nic_target.modem()) {

				s.sub_scope<Frame>([&] (Scope<Frame, Vbox, Frame> &s) {
					s.sub_scope<Vbox>([&] (Scope<Frame, Vbox, Frame, Vbox> &s) {

						if (_nic_target.wifi())
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
		return _nic_target.wifi()
		    && _ap_selector.need_keyboard_focus_for_passphrase();
	}

	bool ap_list_hovered(Hovered_at const &at) const
	{
		return _ap_selector.if_hovered(at, [&] (Hovered_at const &) {
			return _ap_selector.ap_list_shown(); });
	}
};

#endif /* _VIEW__NETWORK_WIDGET_H_ */
