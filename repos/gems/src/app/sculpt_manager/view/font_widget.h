/*
 * \brief  Font-settings widget
 * \author Norman Feske
 * \date   2020-01-30
 */

/*
 * Copyright (C) 2020-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__FONT_WIDGET_H_
#define _VIEW__FONT_WIDGET_H_

#include <view/dialog.h>
#include <model/settings.h>

namespace Sculpt { struct Font_widget; }


struct Sculpt::Font_widget : Widget<Vbox>
{
	enum class Selected_section { NONE, FONT_SIZE };

	Selected_section _selected_section = Selected_section::FONT_SIZE;

	using Font_size = Settings::Font_size;

	static Id _font_size_id(Font_size font_size)
	{
		switch (font_size) {
		case Font_size::SMALL:  return { "Small"  };
		case Font_size::MEDIUM: return { "Medium" };
		case Font_size::LARGE:  return { "Large"  };
		}
		return { };
	}

	struct Font_size_radio : Hosted<Radio_select_button<Font_size>>
	{
		Font_size_radio(Font_size s)
		: Hosted<Radio_select_button<Font_size>>(_font_size_id(s), s) { };
	};

	Font_size_radio const _font_size_items[3] {
		Font_size::SMALL, Font_size::MEDIUM, Font_size::LARGE };

	using Hosted_choice = Hosted<Vbox, Choice<Selected_section>>;

	Hosted_choice const _font_size_choice { Id { "Font size" },
	                                        Selected_section::FONT_SIZE };

	void view(Scope<Vbox> &s, Settings const &settings) const
	{
		unsigned const left_ex = 10, right_ex = 24;

		if (!settings.manual_font_config) {
			Font_size const selected = settings.font_size;
			s.widget(_font_size_choice,
				Hosted_choice::Attr {
					.left_ex = left_ex, .right_ex = right_ex,
					.unfolded      = _selected_section,
					.selected_item = _font_size_id(selected)
				},
				[&] (Hosted_choice::Sub_scope &s) {
					for (auto const &item : _font_size_items)
						s.widget(item, selected);
				});
		}
	}

	struct Action : Interface, Noncopyable
	{
		virtual void select_font_size(Font_size) = 0;
	};

	void click(Clicked_at const &at, Action &action)
	{
		_font_size_choice.propagate(at, _selected_section,
			[&] { /* _selected_section = Selected_section::NONE; */ },
			[&] (Clicked_at const &at) {
				for (auto &item : _font_size_items)
					item.propagate(at, [&] (Font_size s) {
						action.select_font_size(s); });
			});
	}
};

#endif /* _VIEW__FONT_WIDGET_H_ */
