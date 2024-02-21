/*
 * \brief  SIM PIN entry widget
 * \author Norman Feske
 * \date   2022-05-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__PIN_WIDGET_H_
#define _VIEW__PIN_WIDGET_H_

#include <view/dialog.h>
#include <model/sim_pin.h>

namespace Sculpt { struct Pin_widget; }


struct Sculpt::Pin_widget : Widget<Centered_dialog_vbox>
{
	Hosted<Centered_dialog_vbox, Pin_row>
		_rows[3] { { Id { "r1" }, "1", "2", "3" },
		           { Id { "r2" }, "4", "5", "6" },
		           { Id { "r3" }, "7", "8", "9" } },
		_last_row  { Id { "r4" }, "C", "0", "OK" };

	void view(Scope<Centered_dialog_vbox> &s, Blind_sim_pin const &sim_pin) const
	{
		s.sub_scope<Min_ex>(20);
		s.sub_scope<Vgap>();
		s.sub_scope<Hbox>([&] (Scope<Centered_dialog_vbox, Hbox> &s) {
			s.sub_scope<Label>(" Enter SIM PIN ", [&] (auto &s) {
				s.attribute("min_ex", 5);
				s.attribute("font", "title/regular");
			});
			s.sub_scope<Label>(String<64>(" ", sim_pin, " "), [&] (auto &s) {
				s.attribute("min_ex", 5);
				s.attribute("font", "title/regular");
			});
		});
		s.sub_scope<Vgap>();

		for (auto const &row : _rows)
			s.widget(row);

		s.widget(_last_row,
		         Pin_row::Visible { .left = sim_pin.at_least_one_digit(),
		                            .middle = true,
		                            .right = sim_pin.suitable_for_unlock() });
	}

	struct Action : Interface
	{
		virtual void append_sim_pin_digit(Sim_pin::Digit) = 0;
		virtual void remove_last_sim_pin_digit() = 0;
		virtual void confirm_sim_pin() = 0;
	};

	void click(Clicked_at const &at, Blind_sim_pin const &sim_pin, Action &action)
	{
		auto for_each_row = [&] (auto const &fn)
		{
			for (auto &row : _rows) fn(row);
			fn(_last_row);
		};

		for_each_row([&] (auto &row) {
			row.propagate(at, [&] (auto const &label) {

				for (unsigned i = 0; i < 10; i++)
					if (String<10>(i) == label)
						action.append_sim_pin_digit(Sim_pin::Digit{i});

				if (label == "C")
					action.remove_last_sim_pin_digit();

				if (label == "OK" && sim_pin.suitable_for_unlock())
					action.confirm_sim_pin();
			});
		});
	}
};

#endif /* _VIEW__PIN_WIDGET_H_ */
