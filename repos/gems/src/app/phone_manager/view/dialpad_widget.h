/*
 * \brief  Dialpad widget
 * \author Norman Feske
 * \date   2022-06-29
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DIALPAD_WIDGET_H_
#define _VIEW__DIALPAD_WIDGET_H_

#include <view/dialog.h>
#include <model/dialed_number.h>

namespace Sculpt { struct Dialpad_widget; }


struct Sculpt::Dialpad_widget : Widget<Centered_dialog_vbox>
{
	Hosted<Centered_dialog_vbox, Pin_row>
		_rows[4] { { Id { "r1" }, "1", "2", "3" },
		           { Id { "r2" }, "4", "5", "6" },
		           { Id { "r3" }, "7", "8", "9" },
		           { Id { "r4" }, "*", "0", "#" } };

	void view(Scope<Centered_dialog_vbox> &s, Dialed_number const &dialed_number) const
	{
		using Text = String<64>;

		Text digits(dialed_number);

		/* if number grows too large, show the tail end */
		if (digits.length() > 28)
			digits = Text("...", Cstring(digits.string() + digits.length() - 27));

		s.sub_scope<Min_ex>(20);
		s.sub_scope<Vgap>();
		s.sub_scope<Button>([&] (Scope<Centered_dialog_vbox, Button> &s) {
			s.attribute("style", "invisible");
			s.sub_scope<Float>([&] (Scope<Centered_dialog_vbox, Button, Float> &s) {
				s.attribute("west", "yes");
				s.sub_scope<Label>("   Dial", [&] (auto &s) {
					s.attribute("font", "title/regular");
					if (digits.length() > 12)
						s.attribute("style", "invisible");
				});
			});
			s.sub_scope<Hbox>([&] (Scope<Centered_dialog_vbox, Button, Hbox> &s) {
				if (digits.length() <= 12)
					s.sub_scope<Min_ex>(16);
				s.sub_scope<Float>([&] (Scope<Centered_dialog_vbox, Button, Hbox, Float> &s) {
					s.sub_scope<Label>(Text(digits), [&] (auto &s) {
						s.attribute("min_ex", 15);
						if (digits.length() < 20)
							s.attribute("font", "title/regular"); });
					s.sub_scope<Label>(" ", [&] (auto &s) {
						s.attribute("font", "title/regular"); });
				});
			});
		});
		s.sub_scope<Vgap>();

		for (auto const &row : _rows)
			s.widget(row);
	}

	struct Action : Interface
	{
		virtual void append_dial_digit(Dialed_number::Digit) = 0;
	};

	void click(Clicked_at const &at, Action &action)
	{
		for (auto &row : _rows)
			row.propagate(at, [&] (auto const &label) {
				Dialed_number::Digit const digit { label.string()[0] };
				action.append_dial_digit(digit); });
	}
};

#endif /* _VIEW__DIALPAD_WIDGET_H_ */
