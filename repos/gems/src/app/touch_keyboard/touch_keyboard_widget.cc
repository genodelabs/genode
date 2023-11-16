/*
 * \brief  Touch-screen keyboard widget
 * \author Norman Feske
 * \date   2022-01-11
 */

/*
 * Copyright (C) 2022-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <touch_keyboard_widget.h>

using namespace Dialog;


void Touch_keyboard_widget::Key::view(Scope<Vbox> &s, Attr const &attr) const
{
	s.widget(_button, [&] (Scope<Button> &s) {

		if (map.length() > 1)
			s.attribute("style", "unimportant");

		if (text.length() == 1)
			s.attribute("style", "invisible");

		s.sub_scope<Vbox>([&] (Scope<Button, Vbox> &s) {

			unsigned const ex = min_ex ? min_ex : attr.default_key_min_ex.value;
			if (ex)
				s.sub_scope<Min_ex>(ex);

			s.sub_scope<Label>(text, [&] (auto &s) {
				if (small)
					s.attribute("font", "annotation/regular"); });
		});
	});
}


void Touch_keyboard_widget::Row::view(Scope<Hbox> &s, Attr const &attr) const
{
	keys.for_each([&] (Hosted_key const &key) {
		s.widget(key, Key::Attr {
			.default_key_min_ex = attr.default_key_min_ex }); });
}


void Touch_keyboard_widget::view(Scope<Vbox> &s) const
{
	_with_current_map(*this, [&] (Map const &map) {
		map.rows.for_each([&] (Map::Hosted_row const &row) {
			s.widget(row, Row::Attr {
				.default_key_min_ex = _default_key_min_ex }); }); });
}


void Touch_keyboard_widget::click(Clicked_at const &at)
{
	Key::Map next_map = _current_map;

	_with_current_map(*this, [&] (Map &map) {
		map.rows.for_each([&] (Map::Hosted_row &row) {
			row.propagate(at, [&] (Key &key) {

				_emit_on_clack = key.emit;

				if (key.map.length() > 1)
					next_map = key.map;
			});
		});
	});

	_current_map = next_map;
}
