/*
 * \brief  Event-filter widget
 * \author Norman Feske
 * \date   2020-01-30
 */

/*
 * Copyright (C) 2020-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__EVENT_FILTER_WIDGET_H_
#define _VIEW__EVENT_FILTER_WIDGET_H_

#include <view/dialog.h>
#include <model/settings.h>

namespace Sculpt { struct Event_filter_widget; }


struct Sculpt::Event_filter_widget : Widget<Vbox>
{
	enum class Selected_section { NONE, KEYBAORD };

	Selected_section _selected_section = Selected_section::KEYBAORD;

	using Keyboard_layout = String<32>;

	using Keyboard_radio = Hosted<Radio_select_button<Keyboard_layout>>;

	using Hosted_choice = Hosted<Vbox, Choice<Selected_section>>;

	Hosted_choice const
		_keyboard_layout_choice { Id { "Keyboard" }, Selected_section::KEYBAORD };

	struct Attr { Keyboard_layout keyboard_layout; };

	void view(Scope<Vbox> &s, Attr const attr) const
	{
		unsigned const left_ex = 10, right_ex = 24;

		if (attr.keyboard_layout.length() > 1) {
			s.widget(_keyboard_layout_choice,
				Hosted_choice::Attr {
					.left_ex = left_ex, .right_ex = right_ex,
					.unfolded      = _selected_section,
					.selected_item = attr.keyboard_layout
				},
				[&] (Hosted_choice::Sub_scope &s) {
					using Layout = Settings::Keyboard_layout;
					Layout::for_each([&] (Layout const &layout) {
						s.widget(Keyboard_radio { Id { layout.name }, layout.name },
						         attr.keyboard_layout);
					});
				});
		}
	}

	struct Action : Interface, Noncopyable
	{
		virtual void select_keyboard_layout(Keyboard_layout const &) = 0;
	};

	void click(Clicked_at const &at, Action &action)
	{
		_keyboard_layout_choice.propagate(at, _selected_section,
			[&] { /* _selected_section = Selected_section::NONE; */ },
			[&] (Clicked_at const &at) {
				Id const id = at.matching_id<Keyboard_radio>();
				if (id.valid())
					action.select_keyboard_layout(id.value);
			});
	}
};

#endif /* _VIEW__EVENT_FILTER_WIDGET_H_ */
