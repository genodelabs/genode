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
	bool _monitor = false;
	bool _wait    = false;
	bool _wx      = false;

	Hoverable_item _item { };

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

		if (clicked == "monitor") _monitor = !_monitor;
		if (clicked == "wx")      _wx      = !_wx;
		if (clicked == "wait")    _wait    = !_wait;

		/* "wx" depends on "monitor", "wait" depends on "wx" */
		_wx   &= _monitor;
		_wait &= _wx;

		component.wx      = _wx;
		component.monitor = _monitor;
		component.wait    = _wait;
	}

	void _gen_checkbox(Xml_generator &xml, Start_name const &name,
	                   Component::Info const &text, bool selected) const
	{
		gen_named_node(xml, "hbox", name, [&] {

			gen_named_node(xml, "float", "left", [&] {
				xml.attribute("west", "yes");

				xml.node("hbox", [&] {

					gen_named_node(xml, "button", "button", [&] {

						if (selected)
							xml.attribute("selected", "yes");

						xml.attribute("style", "checkbox");
						_item.gen_hovered_attr(xml, name);
						xml.node("hbox", [&] { });
					});
					gen_named_node(xml, "label", "name", [&] {
						xml.attribute("text", Path(" ", text)); });
				});
			});

			gen_named_node(xml, "hbox", "right", [&] { });
		});
	}

	void generate(Xml_generator &xml) const override
	{
		xml.node("vbox", [&] {
			_gen_checkbox(xml, "monitor", "Debug", _monitor);

			if (_monitor)
				_gen_checkbox(xml, "wx", "Allow code patching", _wx);

			if (_wx)
				_gen_checkbox(xml, "wait", "Wait for GDB", _wait);
		});
	}

	void reset() override
	{
		_item._hovered = Hoverable_item::Id();
		_monitor = _wait = _wx = false;
	}
};

#endif /* _VIEW__DEBUG_DIALOG_H_ */
