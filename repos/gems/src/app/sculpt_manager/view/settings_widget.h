/*
 * \brief  Settings widget
 * \author Norman Feske
 * \date   2020-01-30
 */

/*
 * Copyright (C) 2020-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SETTINGS_WIDGET_H_
#define _VIEW__SETTINGS_WIDGET_H_

#include <view/dialog.h>
#include <model/settings.h>

namespace Sculpt { struct Settings_widget; }


struct Sculpt::Settings_widget : Widget<Vbox>
{
	Settings const &_settings;

	enum class Selected_section { NONE, FONT_SIZE, KEYBAORD };

	Selected_section _selected_section = Selected_section::NONE;

	using Font_size       = Settings::Font_size;
	using Keyboard_layout = Settings::Keyboard_layout;

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

	using Keyboard_radio = Hosted<Radio_select_button<Keyboard_layout::Name>>;

	using Hosted_choice = Hosted<Vbox, Choice<Selected_section>>;

	Hosted_choice const
		_font_size_choice       { Id { "Font size" }, Selected_section::FONT_SIZE },
		_keyboard_layout_choice { Id { "Keyboard"  }, Selected_section::KEYBAORD  };

	Settings_widget(Settings const &settings) : _settings(settings) { }

	void view(Scope<Vbox> &s) const
	{
		unsigned const left_ex = 10, right_ex = 24;

		if (!_settings.manual_fonts_config) {
			Font_size const selected = _settings.font_size;
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

		if (!_settings.manual_event_filter_config) {
			s.widget(_keyboard_layout_choice,
				Hosted_choice::Attr {
					.left_ex = left_ex, .right_ex = right_ex,
					.unfolded      = _selected_section,
					.selected_item = _settings.keyboard_layout
				},
				[&] (Hosted_choice::Sub_scope &s) {
					Keyboard_layout::for_each([&] (Keyboard_layout const &layout) {
						s.widget(Keyboard_radio { Id { layout.name }, layout.name },
						         _settings.keyboard_layout);
					});
				});
		}
	}

	struct Action : Interface, Noncopyable
	{
		virtual void select_font_size(Font_size) = 0;
		virtual void select_keyboard_layout(Keyboard_layout::Name const &) = 0;
	};

	void click(Clicked_at const &at, Action &action)
	{
		_font_size_choice.propagate(at, _selected_section,
			[&] { _selected_section = Selected_section::NONE; },
			[&] (Clicked_at const &at) {
				for (auto &item : _font_size_items)
					item.propagate(at, [&] (Font_size s) {
						action.select_font_size(s); });
			});

		_keyboard_layout_choice.propagate(at, _selected_section,
			[&] { _selected_section = Selected_section::NONE; },
			[&] (Clicked_at const &at) {
				Id const id = at.matching_id<Keyboard_radio>();
				if (id.valid())
					action.select_keyboard_layout(id.value);
			});
	}
};

#endif /* _VIEW__SETTINGS_WIDGET_H_ */
