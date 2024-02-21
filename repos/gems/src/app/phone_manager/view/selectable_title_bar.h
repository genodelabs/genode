/*
 * \brief  Section widget
 * \author Norman Feske
 * \date   2022-05-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SELECTABLE_TITLE_BAR_H_
#define _VIEW__SELECTABLE_TITLE_BAR_H_

#include <view/dialog.h>

namespace Sculpt { template <typename> struct Selectable_title_bar; }


template <typename ENUM>
struct Sculpt::Selectable_title_bar : Widget<Vbox>
{
	ENUM const &_selected_value;

	ENUM const value;

	bool _minimized() const { return (_selected_value != value)
	                              && (_selected_value != ENUM::NONE); }

	Selectable_title_bar(ENUM const &selected_value, ENUM value)
	: _selected_value(selected_value), value(value) { }

	void view(Scope<Vbox> &s, auto const &status_fn) const
	{
		Id const &id = s.id;

		s.sub_scope<Float>([&] (Scope<Vbox, Float> &s) {
			s.attribute("east", "yes");
			s.attribute("west", "yes");

			bool const hovered = s.hovered() && (!s.dragged() || selected());

			s.sub_scope<Button>([&] (Scope<Vbox, Float, Button> &s) {

				if (selected()) s.attribute("selected", "yes");
				if (hovered)    s.attribute("hovered",  "yes");

				if (_minimized())
					s.attribute("style", "unimportant");

				s.sub_scope<Hbox>([&] (Scope<Vbox, Float, Button, Hbox> &s) {
					s.sub_scope<Vbox>(id, [&] (Scope<Vbox, Float, Button, Hbox, Vbox> &s) {
						s.sub_scope<Min_ex>(12);
						s.sub_scope<Label>(id.value, [&] (auto &s) {
						if (_minimized())
								s.attribute("font", "annotation/regular");
							else
								s.attribute("font", "title/regular");
						});
					});

					s.sub_scope<Vbox>([&] (Scope<Vbox, Float, Button, Hbox, Vbox> &s) {
						s.sub_scope<Min_ex>(12);
						status_fn(s); });
				});
			});
		});
	}

	void click(Clicked_at const &, auto const &fn) { fn(); }

	void view_status(auto &s, auto const &text) const
	{
		s.template sub_scope<Label>(text, [&] (auto &s) {
			if (_minimized())
				s.attribute("font", "annotation/regular"); });
	}

	bool selected() const { return _selected_value == value; }
};

#endif /* _VIEW__SECTION_DIALOG_H_ */
