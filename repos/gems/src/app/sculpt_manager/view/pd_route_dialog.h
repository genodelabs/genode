/*
 * \brief  PD/CPU route assignment dialog
 * \author Alexander Boettcher
 * \date   2021-02-26
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__PD_ROUTE_DIALOG_H_
#define _VIEW__PD_ROUTE_DIALOG_H_

/* Genode includes */
#include <os/reporter.h>
#include <depot/archive.h>

/* local includes */
#include <xml.h>
#include <model/component.h>
#include <model/runtime_config.h>
#include <view/dialog.h>

namespace Sculpt { struct Pd_route_dialog; }


struct Sculpt::Pd_route_dialog : Noncopyable, Deprecated_dialog
{
	Route          _route         { "<pd/>" };
	Hoverable_item _route_item    { };
	bool           _menu_selected { false };

	Runtime_config const &_runtime_config;

	Pd_route_dialog(Runtime_config const &runtime_config)
	:
		_runtime_config(runtime_config)
	{ }

	Hover_result hover(Xml_node hover_node) override
	{
		Deprecated_dialog::Hover_result const hover_result = hover(hover_node);
		return hover_result;
	}

	template <typename... ARGS>
	Hover_result hover(Xml_node hover, ARGS &&... args)
	{
		Deprecated_dialog::Hover_result const hover_result = Deprecated_dialog::any_hover_changed(
			_route_item.match(hover, args...));

		return hover_result;
	}

	void click(Component &);

	void generate(Xml_generator &xml) const override;

	void reset() override
	{
		if (_route.selected_service.constructed())
			_route.selected_service.destruct();
		_route_item._hovered = Hoverable_item::Id();
		_menu_selected = false;
	}

	void click()
	{
		if (_route_item.hovered("pd_route"))
			_menu_selected = true;
	}

	void _gen_route_entry(Xml_generator &xml,
	                      Start_name const &name,
	                      Start_name const &text,
	                      bool selected, char const *style = "radio") const
	{
		gen_named_node(xml, "hbox", name, [&] () {

			gen_named_node(xml, "float", "left", [&] () {
				xml.attribute("west", "yes");

				xml.node("hbox", [&] () {
					gen_named_node(xml, "button", "button", [&] () {

						if (selected)
							xml.attribute("selected", "yes");

						xml.attribute("style", style);
						_route_item.gen_hovered_attr(xml, name);

						xml.node("hbox", [&] () { });
					});
					gen_named_node(xml, "label", "name", [&] () {
						xml.attribute("text", Path(" ", text)); });
				});
			});

			gen_named_node(xml, "hbox", "right", [&] () { });
		});
	}
};

#endif /* _VIEW__PD_ROUTE_DIALOG_H_ */
