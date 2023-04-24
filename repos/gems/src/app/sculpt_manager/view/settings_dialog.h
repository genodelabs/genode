/*
 * \brief  Settings dialog
 * \author Norman Feske
 * \date   2020-01-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SETTINGS_DIALOG_H_
#define _VIEW__SETTINGS_DIALOG_H_

#include <view/dialog.h>
#include <view/radio_choice_dialog.h>
#include <model/settings.h>

namespace Sculpt { struct Settings_dialog; }


struct Sculpt::Settings_dialog : Noncopyable, Dialog
{
	Settings const &_settings;

	Hoverable_item _section { };

	Radio_choice_dialog::Min_ex const _ratio { .left = 10, .right = 24 };

	Radio_choice_dialog _font_size_choice       { "Font size", _ratio };
	Radio_choice_dialog _keyboard_layout_choice { "Keyboard",  _ratio };

	static Radio_choice_dialog::Id _font_size_id(Settings::Font_size font_size)
	{
		switch (font_size) {
		case Settings::Font_size::SMALL:  return "Small";
		case Settings::Font_size::MEDIUM: return "Medium";
		case Settings::Font_size::LARGE:  return "Large";
		}
		return Radio_choice_dialog::Id();
	}

	static Settings::Font_size _font_size(Radio_choice_dialog::Id id)
	{
		if (id == "Small")  return Settings::Font_size::SMALL;
		if (id == "Medium") return Settings::Font_size::MEDIUM;
		if (id == "Large")  return Settings::Font_size::LARGE;

		return Settings::Font_size::MEDIUM;
	}

	Hover_result hover(Xml_node hover) override
	{
		return any_hover_changed(
			_section.match(hover, "frame", "vbox", "hbox", "name"),
			_font_size_choice.match_sub_dialog(hover, "frame", "vbox"),
			_keyboard_layout_choice.match_sub_dialog(hover, "frame", "vbox"));
	}

	void reset() override { }

	struct Action : Interface, Noncopyable
	{
		virtual void select_font_size(Settings::Font_size) = 0;

		virtual void select_keyboard_layout(Settings::Keyboard_layout::Name const &) = 0;
	};

	void generate(Xml_generator &xml) const override
	{
		gen_named_node(xml, "frame", "settings", [&] () {
			xml.node("vbox", [&] () {

				using Choice = Radio_choice_dialog::Choice;

				if (!_settings.manual_fonts_config) {
					_font_size_choice.generate(xml, _font_size_id(_settings.font_size),
					                           [&] (Choice const &choice) {
						choice.generate("Small");
						choice.generate("Medium");
						choice.generate("Large");
					});
				}

				if (!_settings.manual_event_filter_config) {
					_keyboard_layout_choice.generate(xml, _settings.keyboard_layout,
					                                 [&] (Choice const &choice) {
						using Keyboard_layout = Settings::Keyboard_layout;
						Keyboard_layout::for_each([&] (Keyboard_layout const &layout) {
							choice.generate(layout.name); });
					});
				}
			});
		});
	}

	Click_result click(Action &action)
	{
		_font_size_choice.reset();
		_keyboard_layout_choice.reset();

		Click_result result = Click_result::IGNORED;

		auto handle_section = [&] (Radio_choice_dialog &dialog, auto fn_clicked)
		{
			if (result == Click_result::CONSUMED || !_section.hovered(dialog._id))
				return;

			/* unfold radio choice */
			dialog.click();

			auto selection = dialog.hovered_choice();
			if (selection == "")
				return;

			fn_clicked(selection);

			result = Click_result::CONSUMED;
		};

		handle_section(_font_size_choice, [&] (auto selection) {
			action.select_font_size(_font_size(selection)); });

		handle_section(_keyboard_layout_choice, [&] (auto selection) {
			using Keyboard_layout = Settings::Keyboard_layout;
			Keyboard_layout::for_each([&] (Keyboard_layout const &layout) {
				if (selection == layout.name)
					action.select_keyboard_layout(selection); }); });

		return result;
	}

	Settings_dialog(Settings const &settings) : _settings(settings) { }
};

#endif /* _VIEW__SETTINGS_DIALOG_H_ */
