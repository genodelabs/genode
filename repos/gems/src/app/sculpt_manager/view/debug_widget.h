/*
 * \brief  Debug-options widget
 * \author Norman Feske
 * \date   2023-10-30
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DEBUG_WIDGET_H_
#define _VIEW__DEBUG_WIDGET_H_

/* local includes */
#include <model/component.h>
#include <view/dialog.h>

namespace Sculpt { struct Debug_widget; }


struct Sculpt::Debug_widget : Widget<Vbox>
{
	Hosted<Vbox, Menu_entry> _monitor { Id { "monitor" } },
	                         _wx      { Id { "wx"      } },
	                         _wait    { Id { "wait"    } };

	void click(Clicked_at const &at, Component &component)
	{
		_monitor.propagate(at, [&] { component.monitor = !component.monitor; });
		_wx     .propagate(at, [&] { component.wx      = !component.wx;      });
		_wait   .propagate(at, [&] { component.wait    = !component.wait;    });

		/* "wx" depends on "monitor", "wait" depends on "wx" */
		component.wx   &= component.monitor;
		component.wait &= component.wx;
	}

	void view(Scope<Vbox> &s, Component const &component) const
	{
		s.widget(_monitor, component.monitor, "Debug", "checkbox");

		if (component.monitor)
			s.widget(_wx, component.wx, "Allow code patching", "checkbox");

		if (component.wx)
			s.widget(_wait, component.wait, "Wait for GDB", "checkbox");
	}
};

#endif /* _VIEW__DEBUG_WIDGET_H_ */
