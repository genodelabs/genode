/*
 * \brief  Dialog for the deploy presets
 * \author Norman Feske
 * \date   2023-01-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SOFTWARE_PRESETS_DIALOG_H_
#define _VIEW__SOFTWARE_PRESETS_DIALOG_H_

#include <model/presets.h>
#include <view/dialog.h>
#include <string.h>

namespace Sculpt { struct Software_presets_dialog; }


struct Sculpt::Software_presets_dialog
{
	Presets const &_presets;

	struct Action : Interface
	{
		virtual void load_deploy_preset(Presets::Info::Name const &) = 0;
	};

	Action &_action;

	using Hover_result = Hoverable_item::Hover_result;

	Presets::Info::Name _selected { };

	Hoverable_item   _item      { };
	Activatable_item _operation { };

	Software_presets_dialog(Presets const &presets, Action &action)
	:
		_presets(presets), _action(action)
	{ }

	using Name = Presets::Info::Name;

	void _gen_horizontal_spacer(Xml_generator &xml) const
	{
		gen_named_node(xml, "label", "spacer", [&] {
			xml.attribute("min_ex", 35); });
	}

	void _gen_preset(Xml_generator &xml, Presets::Info const &preset) const
	{
		gen_named_node(xml, "vbox", preset.name, [&] () {

			gen_named_node(xml, "hbox", preset.name, [&] () {

				gen_named_node(xml, "float", "left", [&] () {
					xml.attribute("west", "yes");

					xml.node("hbox", [&] () {
						gen_named_node(xml, "float", "radio", [&] () {
							gen_named_node(xml, "button", "button", [&] () {

								_item.gen_hovered_attr(xml, preset.name);

								if (_selected == preset.name)
									xml.attribute("selected", "yes");

								xml.attribute("style", "radio");

								xml.node("hbox", [&] () { });
							});
						});

						gen_named_node(xml, "label", "name", [&] () {
							xml.attribute("text", Name(" ", Pretty(preset.name))); });

						gen_item_vspace(xml, "vspace");
					});
				});
			});

			if (_selected != preset.name)
				return;

			auto vspacer = [&] (auto name)
			{
				gen_named_node(xml, "label", name, [&] () {
					xml.attribute("text", " "); });
			};

			vspacer("spacer1");

			gen_named_node(xml, "float", "info", [&] () {
				gen_named_node(xml, "label", "text", [&] () {
					xml.attribute("text", preset.text); }); });

			vspacer("spacer2");

			gen_named_node(xml, "float", "operations", [&] () {
				gen_named_node(xml, "button", "load", [&] () {
					_operation.gen_button_attr(xml, "load");
					gen_named_node(xml, "label", "text", [&] () {
						xml.attribute("text", " Load "); });
				});
			});

			vspacer("spacer3");
		});
	}

	void generate(Xml_generator &xml) const
	{
		if (_presets.available())
			gen_named_node(xml, "float", "presets", [&] {
				xml.node("frame", [&] {
					xml.node("vbox", [&] {
						_gen_horizontal_spacer(xml);
						_presets.for_each([&] (Presets::Info const &info) {
							_gen_preset(xml, info); }); }); }); });
	}

	Hover_result hover(Xml_node hover)
	{
		return Dialog::any_hover_changed(
			_item.match     (hover, "float", "frame", "vbox", "vbox", "hbox", "name"),
			_operation.match(hover, "float", "frame", "vbox", "vbox", "float", "button", "name")
		);
	}

	bool hovered() const { return _item._hovered.valid(); }

	void click()
	{
		if (_item._hovered.length() > 1)
			_selected = _item._hovered;

		if (_operation.hovered("load"))
			_operation.propose_activation_on_click();
	}

	void clack()
	{
		if (_selected.length() <= 1)
			return;

		_operation.confirm_activation_on_clack();

		if (_operation.activated("load")) {
			_action.load_deploy_preset(_selected);
			_selected = { };
		}

		_operation.reset();
	}
};

#endif /* _VIEW__SOFTWARE_PRESETS_DIALOG_H_ */
