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

	enum class Target { DISCONNECTED, NIC, WIFI, MOBILE };

	struct Action : Interface, Noncopyable
	{
		virtual void nic_target(Target) = 0;
	};

	struct Enabled
	{
		bool nic, wifi, mobile;
		bool any() const { return nic || wifi || mobile; }

		static Enabled from_runtime(Runtime_state const &runtime)
		{
			return { .nic    = runtime.present_in_runtime("nic"),
			         .wifi   = runtime.present_in_runtime("wifi"),
			         .mobile = runtime.present_in_runtime("mobile") };
		}
	};

	struct Target_selector : Widget<Hbox>
	{
		Hosted<Hbox, Toggle_button>
			_local  { Id { "Disconected" } },
			_nic    { Id { "Wired"       } },
			_wifi   { Id { "Wifi"        } },
			_mobile { Id { "Mobile data" } };

		void view(Scope<Hbox> &s, Enabled const enabled, Board_info const &board_info) const
		{
			s.widget(_local, !enabled.any());

			if (board_info.detected.nic || board_info.soc.nic)
				s.widget(_nic, enabled.nic);

			if (board_info.wifi_avail())
				s.widget(_wifi, enabled.wifi);

			if (board_info.soc.modem)
				s.widget(_mobile, enabled.mobile);
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

	void _gen_connected_ap(Generator &, bool) const;

	void view(Scope<Frame> &s, Nic_state const &nic_state,
	          Board_info const &board_info, Enabled enabled, auto const &fn) const
	{
		s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
			s.sub_scope<Min_ex>(35);

			s.widget(_target_selector, enabled, board_info);

			if (enabled.any()) {
				s.sub_scope<Frame>([&] (Scope<Frame, Vbox, Frame> &s) {
					s.sub_scope<Vbox>([&] (Scope<Frame, Vbox, Frame, Vbox> &s) {
						if (nic_state.ready())
							s.sub_scope<Label>(nic_state.ipv4);
						fn(s);
					});
				});
			}
		});
	}

	void click(Clicked_at const &at, Action &action, auto const &fn)
	{
		_target_selector.propagate(at, action);

		fn(at);
	}
};

#endif /* _VIEW__NETWORK_WIDGET_H_ */
