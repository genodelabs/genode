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
#include <model/settings.h>

namespace Sculpt { struct Settings_dialog; }


struct Sculpt::Settings_dialog : Noncopyable, Dialog
{
	Font_size const &_current_font_size;

	Hoverable_item _item   { };

	static Hoverable_item::Id _font_size_id(Font_size font_size)
	{
		switch (font_size) {
		case Font_size::SMALL:  return "small";
		case Font_size::MEDIUM: return "medium";
		case Font_size::LARGE:  return "large";
		}
		return Hoverable_item::Id();
	}

	Hover_result hover(Xml_node hover) override
	{
		return any_hover_changed(
			_item.match(hover, "frame", "vbox", "hbox", "button", "name"));
	}

	void reset() override { }

	struct Action : Interface, Noncopyable
	{
		virtual void select_font_size(Font_size) = 0;
	};

	void generate(Xml_generator &xml) const override
	{
		gen_named_node(xml, "frame", "network", [&] () {
			xml.node("vbox", [&] () {
				gen_named_node(xml, "hbox", "font_size", [&] () {
					gen_named_node(xml, "label", "label", [&] () {
						xml.attribute("text", " Font size "); });

					auto gen_font_size_button = [&] (Font_size font_size)
					{
						Label const label = _font_size_id(font_size);
						gen_named_node(xml, "button", label, [&] () {
							_item.gen_hovered_attr(xml, label);

							if (font_size == _current_font_size)
								xml.attribute("selected", true);

							xml.node("label", [&] () {
								xml.attribute("text", label); });
						});
					};

					gen_font_size_button(Font_size::SMALL);
					gen_font_size_button(Font_size::MEDIUM);
					gen_font_size_button(Font_size::LARGE);
				});
			});
		});
	}

	Click_result click(Action &action)
	{
		Click_result result = Click_result::IGNORED;

		auto apply_font_size = [&] (Font_size font_size)
		{
			if (_item.hovered(_font_size_id(font_size))) {
				action.select_font_size(font_size);
				result = Click_result::CONSUMED;
			}
		};

		apply_font_size(Font_size::SMALL);
		apply_font_size(Font_size::MEDIUM);
		apply_font_size(Font_size::LARGE);

		return result;
	}

	Settings_dialog(Font_size const &current_font_size)
	:
		_current_font_size(current_font_size)
	{ }
};

#endif /* _VIEW__RAM_FS_DIALOG_H_ */
