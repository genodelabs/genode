/*
 * \brief  Resource assignment dialog
 * \author Alexander Boettcher
 * \author Norman Feske
 * \date   2020-07-22
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__RESOURCE_DIALOG_H_
#define _VIEW__RESOURCE_DIALOG_H_

/* Genode includes */
#include <os/reporter.h>
#include <depot/archive.h>

/* local includes */
#include <xml.h>
#include <model/component.h>
#include <view/dialog.h>
#include <view/selectable_item.h>

namespace Sculpt { struct Resource_dialog; }


struct Sculpt::Resource_dialog : Noncopyable, Deprecated_dialog
{
	Affinity::Space const _space;
	Affinity::Location    _location;
	Hoverable_item        _space_item { };
	Selectable_item       _priority_item { };

	static char const *_priority_id(Priority priority)
	{
		switch (priority) {
		case Priority::DRIVER:     return "driver";
		case Priority::MULTIMEDIA: return "multimedia";
		case Priority::BACKGROUND: return "background";
		default:                   break;
		};
		return "default";
	}

	static Hoverable_item::Id _cpu_id(unsigned x, unsigned y)
	{
		return Hoverable_item::Id("cpu", x, "x", y);
	}

	Resource_dialog(Affinity::Space space, Affinity::Location location,
	                Priority priority)
	:
		_space(space), _location(location)
	{
		_priority_item.select(_priority_id(priority));
	}

	void _gen_affinity_section(Xml_generator &) const;
	void _gen_priority_section(Xml_generator &) const;

	Hover_result hover(Xml_node hover) override
	{
		return Deprecated_dialog::any_hover_changed(
			_space_item.match(hover,
			                  "vbox", "float", "hbox",        /* _gen_dialog_section */
			                  "vbox", "hbox", "vbox", "hbox", /* _gen_affinity_section */
			                  "vbox", "button", "name"        /* gen_cell_cpu */
			),
			_priority_item.match(hover,
			                     "vbox", "float", "hbox",     /* _gen_dialog_section */
			                     "vbox", "hbox", "float", "hbox", "name"));
	}

	void click(Component &);

	void _click_space   (Component &, Hoverable_item::Id);
	void _click_priority(Component &, Hoverable_item::Id);

	template <typename FN>
	void _gen_dialog_section(Xml_generator &xml, Hoverable_item::Id id,
	                         char const *text, FN const &gen_content_fn) const
	{
		gen_named_node(xml, "float", id, [&] () {
			xml.attribute("west", "yes");

			xml.node("hbox", [&] () {

				gen_named_node(xml, "vbox", "title", [&] () {
					xml.node("float", [&] () {

						xml.attribute("north", "yes");
						xml.attribute("west",  "yes");

						xml.node("hbox", [&] () {

							/*
							 * The button is used to vertically align the "Priority"
							 * label with the text of the first radio button.
							 * The leading space is used to horizontally align
							 * the label with the "Resource assignment ..." dialog
							 * title.
							 */
							gen_named_node(xml, "button", "spacer", [&] () {
								xml.attribute("style", "invisible");
								xml.node("hbox", [&] () { }); });

							gen_named_node(xml, "label", "label", [&] () {
								xml.attribute("text", String<32>(" ", text)); });
						});
					});

					gen_named_node(xml, "label", "spacer", [&] () {
						xml.attribute("min_ex", 11); });
				});

				gen_content_fn();
			});
		});
	}

	void generate(Xml_generator &xml) const override
	{
		auto gen_vspacer = [&] (auto id) {
			gen_named_node(xml, "label", id, [&] () {
				xml.attribute("text", "");
				xml.attribute("font", "annotation/regular"); }); };

		xml.node("vbox", [&] () {
			gen_vspacer("spacer1");
			_gen_affinity_section(xml);
			gen_vspacer("spacer2");
			_gen_priority_section(xml);
			gen_vspacer("spacer3");
		});
	}

	void reset() override
	{
		_space_item._hovered = Hoverable_item::Id();
		_priority_item.reset();
		_location = Affinity::Location();
	}
};

#endif /* _VIEW__RESOURCE_DIALOG_H_ */
