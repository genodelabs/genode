/*
 * \brief  Resource-assignment widget
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

#include <view/resource_widget.h>

using namespace Sculpt;


void Resource_widget::Affinity_selector::view(Scope<Vbox> &s,
                                              Affinity::Space    const &space,
                                              Affinity::Location const &location) const
{
	auto view_hyperthread_index = [] (auto &s, unsigned index) {
		s.template sub_scope<Label>(String<8>(index), [&] (auto &s) {
			s.attribute("font", "annotation/regular");
			s.attribute("min_ex", 2); }); };

	auto view_cell_hspacer = [] (auto &s) {
		s.template sub_scope<Min_ex>(5); };

	auto view_cell_cpu = [&] (auto &s, Id const &id, bool selected) {
		s.template sub_scope<Vbox>(id, [&] (auto &s) {
			view_cell_hspacer(s);
			s.template sub_scope<Float>(id, [&] (auto &s) {
				s.template sub_scope<Button>([&] (auto &s) {
					s.attribute("style", "checkbox");
					s.attribute("selected", selected);
					s.template sub_scope<Hbox>(); }); }); }); };

	auto view_cpu_index = [&] (auto &s, unsigned index) {
		s.template sub_scope<Vbox>([&] (auto &s) {
			s.template sub_scope<Annotation>(String<8>(index));
			view_cell_hspacer(s); }); };

	auto view_leftaligned = [] (auto &s, auto text) {
		s.template sub_scope<Float>([&] (auto &s) {
			s.attribute("west", "yes");
			s.template sub_scope<Annotation>(text); }); };

	auto selected = [] (Affinity::Location location, unsigned x, unsigned y)
	{
		return (unsigned(location.xpos()) <= x)
		    && (x < location.xpos() + location.width())
		    && (unsigned(location.ypos()) <= y)
		    && (y < location.ypos() + location.height());
	};

	bool const have_hyperthreads = (space.height() > 1);

	s.sub_scope<Hbox>([&] (Scope<Vbox, Hbox> &s) {
		s.sub_scope<Vbox>([&] (Scope<Vbox, Hbox, Vbox> &s) {

			for (unsigned y = 0; y < space.height(); y++) {
				s.sub_scope<Hbox>(Id { { y } }, [&] (Scope<Vbox, Hbox, Vbox, Hbox> &s) {
					for (unsigned x = 0; x < space.width(); x++)
						view_cell_cpu(s, Id { Id::Value(x) }, selected(location, x, y));

					if (have_hyperthreads)
						s.sub_scope<Float>([&] (Scope<Vbox, Hbox, Vbox, Hbox, Float> &s) {
							view_hyperthread_index(s, y ); });
				});
			}
		});

		if (have_hyperthreads)
			s.sub_scope<Float>([&] (Scope<Vbox, Hbox, Float> &s) {
				s.sub_scope<Vbox>([&] (Scope<Vbox, Hbox, Float, Vbox> &s) {
					view_leftaligned(s, "Hyper");
					view_leftaligned(s, "threads"); }); });
	});

	s.sub_scope<Float>([&] (Scope<Vbox, Float> &s) {
		s.attribute("west", "yes");
		s.sub_scope<Vbox>([&] (Scope<Vbox, Float, Vbox> &s) {
			s.sub_scope<Hbox>([&] (Scope<Vbox, Float, Vbox, Hbox> &s) {
				for (unsigned x = 0; x < space.width(); x++)
					view_cpu_index(s, x);
			});
			s.sub_scope<Annotation>("Cores");
		});
	});
}


void Resource_widget::Affinity_selector::click(Clicked_at      const &at,
                                               Affinity::Space const &space,
                                               Affinity::Location    &location)
{
	Id const x_id = at.matching_id<Vbox, Hbox, Vbox, Hbox, Vbox>(),
	         y_id = at.matching_id<Vbox, Hbox, Vbox, Hbox>();

	unsigned x = ~0, y = ~0;

	ascii_to(x_id.value.string(), x);
	ascii_to(y_id.value.string(), y);

	if (x >= space.width() || y >= space.height())
		return;

	unsigned loc_x = location.xpos();
	unsigned loc_y = location.ypos();
	unsigned loc_w = location.width();
	unsigned loc_h = location.height();

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

	location = Affinity::Location(loc_x, loc_y, loc_w, loc_h);
}
