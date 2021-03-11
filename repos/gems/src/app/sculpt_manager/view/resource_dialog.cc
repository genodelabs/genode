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

#include <view/resource_dialog.h>

using namespace Sculpt;


void Resource_dialog::_gen_affinity_section(Xml_generator &xml) const
{
	unsigned const CELL_WIDTH_EX = 4;

	using Id = Hoverable_item::Id;

	auto gen_small_label = [] (Xml_generator &xml, auto id, auto fn) {
		gen_named_node(xml, "label", id, [&] () {
			xml.attribute("font", "annotation/regular");
			fn(); }); };

	auto gen_cell_hspacer = [&] (Xml_generator &xml, auto id) {
		gen_small_label(xml, id, [&] () {
			xml.attribute("min_ex", CELL_WIDTH_EX); }); };

	auto gen_cell_cpu = [&] (Xml_generator &xml, auto name, bool selected)
	{
		gen_named_node(xml, "vbox",  name, [&] () {

			gen_named_node(xml, "button", name, [&] () {
				xml.attribute("style", "checkbox");
				_space_item.gen_hovered_attr(xml, name);

				if (selected)
					xml.attribute("selected", "yes");

				xml.node("hbox", [&] () { });
			});
			gen_cell_hspacer(xml, "below");
		});
	};

	auto gen_cell_number = [&] (Xml_generator &xml, Id n)
	{
		gen_named_node(xml, "vbox", n, [&] () {
			gen_cell_hspacer(xml, "above");
			gen_named_node(xml, "float", "number", [&] () {
				gen_small_label(xml, "number", [&] () {
					xml.attribute("text", n); }); });
			gen_cell_hspacer(xml, "below");
		});
	};

	auto cpu_selected = [] (Affinity::Location location, unsigned x, unsigned y)
	{
		return (unsigned(location.xpos()) <= x)
		    && (x < location.xpos() + location.width())
		    && (unsigned(location.ypos()) <= y)
		    && (y < location.ypos() + location.height());
	};

	_gen_dialog_section(xml, "affinity", "Affinity", [&] () {

		gen_named_node(xml, "vbox", "selection", [&] () {

			bool const have_hyperthreads = (_space.height() > 1);

			gen_named_node(xml, "hbox", "labeledcores", [&] () {
				gen_named_node(xml, "vbox", "cores", [&] () {

					for (unsigned y = 0; y < _space.height(); y++) {

						gen_named_node(xml, "hbox", Id("row", y), [&] () {

							for (unsigned x = 0; x < _space.width(); x++)
								gen_cell_cpu(xml, _cpu_id(x, y),
								             cpu_selected(_location, x, y));

							if (have_hyperthreads)
								gen_named_node(xml, "float", "number", [&] () {
									gen_small_label(xml, "number", [&] () {
										xml.attribute("min_ex", 2);
										xml.attribute("text", y); }); });
						});
					}
				});
				if (have_hyperthreads) {
					gen_named_node(xml, "float", "hyperthreadslabel", [&] () {
						xml.node("vbox", [&] () {

							unsigned line = 0;

							auto gen_leftaligned = [&] (auto text) {
								gen_named_node(xml, "float", Id(line++), [&] () {
									xml.attribute("west", "yes");
									gen_small_label(xml, "label", [&] () {
										xml.attribute("text", text); }); }); };

							gen_leftaligned("Hyper");
							gen_leftaligned("threads");
						});
					});
				}
			});

			gen_named_node(xml, "float", "corelabels", [&] () {
				xml.attribute("west", "yes");
				xml.node("vbox", [&] () {

					xml.node("hbox", [&] () {
						for (unsigned x = 0; x < _space.width(); x++)
							gen_cell_number(xml, x); });

					gen_small_label(xml, "cores", [&] () {
						xml.attribute("text", "Cores"); });
				});
			});
		});
	});
}


void Resource_dialog::_gen_priority_section(Xml_generator &xml) const
{
	_gen_dialog_section(xml, "priority", "Priority", [&] () {

		gen_named_node(xml, "vbox", "selection", [&] () {

			auto gen_radiobutton = [&] (auto id, auto text)
			{
				gen_named_node(xml, "hbox", id, [&] () {

					gen_named_node(xml, "float", "left", [&] () {
						xml.attribute("west", "yes");

						gen_named_node(xml, "hbox", id, [&] () {
							gen_named_node(xml, "button", "button", [&] () {

								xml.attribute("style", "radio");
								_priority_item.gen_button_attr(xml, id);
								xml.node("hbox", [&] () { });
							});
							gen_named_node(xml, "label", "name", [&] () {
								xml.attribute("text", text);
								xml.attribute("min_ex", 13);
							});
						});
					});

					gen_named_node(xml, "hbox", "right", [&] () { });
				});
			};

			gen_radiobutton("driver",     "Driver");
			gen_radiobutton("multimedia", "Multimedia");
			gen_radiobutton("default",    "Default");
			gen_radiobutton("background", "Background");
		});

		gen_named_node(xml, "hbox", "right", [&] () { });
	});
}


void Resource_dialog::click(Component &component)
{
	if (component.affinity_space.total() <= 1)
		return;

	Hoverable_item::Id const clicked_space = _space_item._hovered;
	if (clicked_space.valid()) {
		_click_space(component, clicked_space);
		return;
	}

	Hoverable_item::Id const clicked_priority = _priority_item._hovered;
	if (clicked_priority.valid()) {
		_click_priority(component, clicked_priority);
		return;
	}
}


void Resource_dialog::_click_space(Component &component, Hoverable_item::Id clicked_space)
{
	for (unsigned y = 0; y < _space.height(); y++) {
		for (unsigned x = 0; x < _space.width(); x++) {
			if (_cpu_id(x, y) != clicked_space)
				continue;

			unsigned loc_x = _location.xpos();
			unsigned loc_y = _location.ypos();
			unsigned loc_w = _location.width();
			unsigned loc_h = _location.height();

			bool handled_x = false;
			bool handled_y = false;

			if (x < loc_x) {
				loc_w += loc_x - x;
				loc_x = x;
				handled_x = true;
			} else if (x >= loc_x + loc_w) {
				loc_w = x - loc_x + 1;
				handled_x = true;
			}

			if (y < loc_y) {
				loc_h += loc_y - y;
				loc_y = y;
				handled_y = true;
			} else if (y >= loc_y + loc_h) {
				loc_h = y - loc_y + 1;
				handled_y = true;
			}

			if (handled_x && !handled_y) {
				handled_y = true;
			} else
			if (handled_y && !handled_x) {
				handled_x = true;
			}

			if (!handled_x) {
				if ((x - loc_x) < (loc_x + loc_w - x)) {
					if (x - loc_x + 1 < loc_w) {
						loc_w -= x - loc_x + 1;
						loc_x = x + 1;
					} else {
						loc_w = loc_x + loc_w - x;
						loc_x = x;
					}
				} else {
					loc_w = x - loc_x;
				}
			}

			if (!handled_y) {
				if ((y - loc_y) < (loc_y + loc_h - y)) {
					if (y - loc_y + 1 < loc_h) {
						loc_h -= y - loc_y + 1;
						loc_y = y + 1;
					} else {
						loc_h = loc_y + loc_h - y;
						loc_y = y;
					}
				} else {
					loc_h = y - loc_y;
				}
			}

			_location = Affinity::Location(loc_x, loc_y,
			                               loc_w, loc_h);

			component.affinity_location = _location;
		}
	}
}


void Resource_dialog::_click_priority(Component &component, Hoverable_item::Id priority)
{
	_priority_item.select(priority);

	/* propagate priority change to component */
	auto priority_value = [] (Hoverable_item::Id string)
	{
		if (string == "background") return Priority::BACKGROUND;
		if (string == "default")    return Priority::DEFAULT;
		if (string == "multimedia") return Priority::MULTIMEDIA;
		if (string == "driver")     return Priority::DRIVER;

		return Priority::BACKGROUND;
	};

	component.priority = priority_value(priority);
}
