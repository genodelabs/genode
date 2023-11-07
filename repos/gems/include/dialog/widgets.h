/*
 * \brief  Widget types
 * \author Norman Feske
 * \date   2023-03-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DIALOG__WIDGETS_H_
#define _INCLUDE__DIALOG__WIDGETS_H_

#include <dialog/sub_scopes.h>

namespace Dialog {

	template <typename> struct Select_button;
	struct Toggle_button;
	struct Action_button;
	struct Deferred_action_button;
}


struct Dialog::Toggle_button : Widget<Button>
{
	template <typename FN>
	void view(Scope<Button> &s, bool selected, FN const &fn) const
	{
		bool const hovered = (s.hovered() && (!s.dragged() || selected));

		if (selected) s.attribute("selected", "yes");
		if (hovered)  s.attribute("hovered",  "yes");

		fn(s);
	}

	void view(Scope<Button> &s, bool selected) const
	{
		view(s, selected, [&] (Scope<Button> &s) {
			s.sub_scope<Dialog::Label>(s.id.value); });
	}

	template <typename FN>
	void click(Clicked_at const &, FN const &toggle_fn) const { toggle_fn(); }
};


template <typename ENUM>
struct Dialog::Select_button : Widget<Button>
{
	ENUM const _value;

	Select_button(ENUM value) : _value(value) { }

	void view(Scope<Button> &s, ENUM selected_value, auto const &fn) const
	{
		bool const selected = (selected_value == _value),
		           hovered  = (s.hovered() && !s.dragged() && !selected);

		if (selected) s.attribute("selected", "yes");
		if (hovered)  s.attribute("hovered",  "yes");

		fn(s);
	}

	void view(Scope<Button> &s, ENUM selected_value) const
	{
		view(s, selected_value, [&] (auto &s) {
			s.template sub_scope<Dialog::Label>(s.id.value); });
	}

	template <typename FN>
	void click(Clicked_at const &, FN const &select_fn) const { select_fn(_value); }
};


struct Dialog::Action_button : Widget<Button>
{
	Event::Seq_number _seq_number { };

	template <typename FN>
	void view(Scope<Button> &s, FN const &fn) const
	{
		bool const selected = _seq_number == s.hover.seq_number,
		           hovered  = (s.hovered() && (!s.dragged() || selected));

		if (selected) s.attribute("selected", "yes");
		if (hovered)  s.attribute("hovered",  "yes");

		fn(s);
	}

	void view(Scope<Button> &s) const
	{
		view(s, [&] (Scope<Button> &s) { s.sub_scope<Label>(s.id.value); });
	}

	template <typename FN>
	void click(Clicked_at const &at, FN const &activate_fn)
	{
		_seq_number = at.seq_number;
		activate_fn();
	}
};


struct Dialog::Deferred_action_button : Widget<Button>
{
	Event::Seq_number _seq_number { };  /* remembered at proposal time */

	template <typename FN>
	void view(Scope<Button> &s, FN const &fn) const
	{
		bool const selected = s.hovered() && s.dragged() && s.hover.matches(_seq_number),
		           hovered  = s.hovered() && (!s.dragged() || selected);

		if (selected) s.attribute("selected", "yes");
		if (hovered)  s.attribute("hovered",  "yes");

		fn(s);
	}

	void view(Scope<Button> &s) const
	{
		view(s, [&] (Scope<Button> &s) { s.sub_scope<Label>(s.id.value); });
	}

	void click(Clicked_at const &at)
	{
		_seq_number = at.seq_number;
	}

	template <typename FN>
	void clack(Clacked_at const &at, FN const &activate_fn)
	{
		if (at.matches(_seq_number))
			activate_fn();
	}
};

#endif /* _INCLUDE__DIALOG__WIDGETS_H_ */
