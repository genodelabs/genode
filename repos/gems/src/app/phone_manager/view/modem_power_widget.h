/*
 * \brief  Modem power-control widget
 * \author Norman Feske
 * \date   2022-05-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__MODEM_POWER_WIDGET_H_
#define _VIEW__MODEM_POWER_WIDGET_H_

#include <view/dialog.h>
#include <model/modem_state.h>

namespace Sculpt { struct Modem_power_widget; }


struct Sculpt::Modem_power_widget : Widget<Frame>
{
	Hosted<Frame, Right_floating_off_on> _power_switch { Id { "power" } };

	void view(Scope<Frame> &s, Modem_state const &state) const
	{
		s.attribute("style", "important");

		s.sub_scope<Left_floating_text>("Modem Power");

		s.widget(_power_switch, Right_floating_off_on::Attr {
			.on        = state.on(),
			.transient = state.transient()
		});
	}

	struct Action : Interface
	{
		virtual void modem_power(bool) = 0;
	};

	void click(Clicked_at const &at, Action &action)
	{
		_power_switch.propagate(at, [&] (bool on) { action.modem_power(on); });
	}
};

#endif /* _VIEW__MODEM_POWER_WIDGET_H_ */
