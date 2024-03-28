/*
 * \brief  Widget for the tabs displayed in the popup dialog
 * \author Norman Feske
 * \date   2024-03-28
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__POPUP_TABS_WIDGET_H_
#define _VIEW__POPUP_TABS_WIDGET_H_

#include <view/dialog.h>

namespace Sculpt { struct Popup_tabs_widget; }


struct Sculpt::Popup_tabs_widget : Widget<Hbox>
{
	enum class Tab { ADD, OPTIONS };

	Tab _selected = Tab::OPTIONS;

	Hosted<Hbox, Select_button<Tab>> const
		_add     { Id { "Add"     }, Tab::ADD     },
		_options { Id { "Options" }, Tab::OPTIONS };

	void view(Scope<Hbox> &s) const
	{
		s.widget(_add,     _selected);
		s.widget(_options, _selected);
	}

	void click(Clicked_at const &at, auto const &fn)
	{
		_add    .propagate(at, [&] (Tab t) { _selected = t; });
		_options.propagate(at, [&] (Tab t) { _selected = t; });

		fn();
	}

	bool options_selected() const { return _selected == Tab::OPTIONS; }
	bool add_selected()     const { return _selected == Tab::ADD;     }
};

#endif /* _VIEW__POPUP_TABS_WIDGET_H_ */
