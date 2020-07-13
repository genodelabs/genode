/*
 * \brief  Resource assignment dialog
 * \author Alexander Boettcher
 * \date   2020-07-22
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <view/resource_dialog.h>

using namespace Sculpt;

void Resource_dialog::_gen_affinity_entry(Xml_generator &xml,
                                          Start_name const &name) const
{
	gen_named_node(xml, "hbox", name, [&] () {
		gen_named_node(xml, "float", "center", [&] () {
			xml.attribute("north", "yes");
			xml.attribute("south", "yes");

			xml.node("vbox", [&] () {
				if (_space.height() > 1) {
					xml.node("label", [&] () {
						xml.attribute("text", String<12>("Cores 0-", _space.width() - 1));
					});
				} else {
					xml.node("label", [&] () {
						xml.attribute("text", String<12>("CPUs 0-", _space.width() - 1));
					});
				}
				for (unsigned y = 0; y < _space.height(); y++) {
					bool const row = unsigned(_location.ypos()) <= y && y < _location.ypos() + _location.height();
					String<12> const row_id("row", y);

					gen_named_node(xml, "hbox", row_id, [&] () {
						if (_space.height() > 1) {
							xml.node("label", [&] () {
								xml.attribute("text", String<12>("Thread ", y));
							});
						}
						for (unsigned x = 0; x < _space.width(); x++) {
							String<12> const name_cpu("cpu", x, "x", y);
							bool const column = unsigned(_location.xpos()) <= x && x < _location.xpos() + _location.width();
							gen_named_node(xml, "button", name_cpu, [&] () {
								if (row && column)
									xml.attribute("selected", "yes");

								xml.attribute("style", "radio");
								_space_item.gen_hovered_attr(xml, name_cpu);
								xml.node("hbox", [&] () { });
							});
						}
					});
				}
			});
		});
	});
}

void Resource_dialog::click(Component &component)
{
	if (component.affinity_space.total() <= 1)
		return;

	Route::Id const clicked_space = _space_item._hovered;
	if (!clicked_space.valid())
		return;

	for (unsigned y = 0; y < component.affinity_space.height(); y++) {
		for (unsigned x = 0; x < component.affinity_space.width(); x++) {
			String<12> const name_cpu("cpu", x, "x", y);
			if (name_cpu != clicked_space)
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
