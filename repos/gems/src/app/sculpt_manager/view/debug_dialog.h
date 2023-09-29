/*
 * \brief  Debug options dialog
 * \author Christian Prochaska
 * \date   2022-09-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DEBUG_DIALOG_H_
#define _VIEW__DEBUG_DIALOG_H_

/* local includes */
#include <xml.h>
#include <model/component.h>
#include <view/dialog.h>

namespace Sculpt { struct Debug_dialog; }


struct Sculpt::Debug_dialog : Noncopyable, Deprecated_dialog
{
	bool _monitor;
	bool _wait;
	bool _wx;

	Hoverable_item _item { };

	Debug_dialog(bool monitor, bool wait, bool wx)
	: _monitor(monitor), _wait(wait), _wx(wx) { }

	Hover_result hover(Xml_node hover) override
	{
		return Deprecated_dialog::any_hover_changed(
			_item.match(hover, "vbox", "hbox", "name"));
	}

	void click(Component &component)
	{
		Hoverable_item::Id const clicked = _item._hovered;

		if (!clicked.valid())
			return;

		if (clicked == "monitor")
			_monitor = component.monitor = !component.monitor;
		else if (clicked == "wait") {
			_wait = component.wait = !component.wait;
			/* wait depends on wx */
			if (_wait)
				_wx = component.wx = true;
		} else if (clicked == "wx") {
			_wx = component.wx = !component.wx;
			/* wait depends on wx */
			if (!_wx)
				_wait = component.wait = false;
		}
	}

	void _gen_menu_entry(Xml_generator &xml, Start_name const &name,
	                     Component::Info const &text, bool selected,
	                     char const *style = "radio") const
	{
		gen_named_node(xml, "hbox", name, [&] () {

			gen_named_node(xml, "float", "left", [&] () {
				xml.attribute("west", "yes");

				xml.node("hbox", [&] () {
					gen_named_node(xml, "button", "button", [&] () {

						if (selected)
							xml.attribute("selected", "yes");

						xml.attribute("style", style);
						_item.gen_hovered_attr(xml, name);
						xml.node("hbox", [&] () { });
					});
					gen_named_node(xml, "label", "name", [&] () {
						xml.attribute("text", Path(" ", text)); });
				});
			});

			gen_named_node(xml, "hbox", "right", [&] () { });
		});
	}

	void generate(Xml_generator &xml) const override
	{
		xml.node("vbox", [&] () {
			_gen_menu_entry(xml, "monitor", "monitor this component", _monitor, "checkbox");
			if (_monitor) {
				_gen_menu_entry(xml, "wait", "  wait for GDB", _wait, "checkbox");
				_gen_menu_entry(xml, "wx", "  map executable segments writeable", _wx, "checkbox");
			}
		});
	}

	void reset() override
	{
		_item._hovered = Hoverable_item::Id();
	}
};

#endif /* _VIEW__DEBUG_DIALOG_H_ */
