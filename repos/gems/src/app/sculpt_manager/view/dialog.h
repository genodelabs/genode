/*
 * \brief  Menu-view dialog handling
 * \author Norman Feske
 * \date   2023-07-12
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DIALOG_H_
#define _VIEW__DIALOG_H_

#include <dialog/widgets.h>

namespace Sculpt { using namespace Dialog; }


namespace Dialog {

	struct Annotation;
	struct Left_annotation;
	struct Left_right_annotation;
	struct Left_floating_text;
	struct Left_floating_hbox;
	struct Right_floating_hbox;
	struct Vgap;
	struct Centered_info_vbox;
	struct Centered_dialog_vbox;
	struct Titled_frame;
	struct Pin_button;
	struct Pin_row;
	struct Doublechecked_action_button;
	template <typename> struct Radio_select_button;
}


struct Dialog::Annotation : Sub_scope
{
	template <typename SCOPE, typename TEXT>
	static void sub_node(SCOPE &s, TEXT const &text)
	{
		s.sub_node("label", [&] {
			s.attribute("text", text);
			s.attribute("font", "annotation/regular"); });
	}
};


struct Dialog::Left_annotation : Sub_scope
{
	template <typename SCOPE, typename TEXT>
	static void view_sub_scope(SCOPE &s, TEXT const &text)
	{
		s.node("hbox", [&] {
			s.sub_node("float", [&] () {
				s.attribute("west", "yes");
				Annotation::sub_node(s, text); }); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &, FN const &) { }
};


struct Dialog::Left_right_annotation : Sub_scope
{
	template <typename SCOPE, typename LEFT_TEXT, typename RIGHT_TEXT>
	static void view_sub_scope(SCOPE &s, LEFT_TEXT const &left, RIGHT_TEXT const &right)
	{
		s.node("hbox", [&] {
			s.named_sub_node("float", "left", [&] () {
				s.attribute("west", "yes");
				Annotation::sub_node(s, left); });

			s.named_sub_node("float", "right", [&] () {
				s.attribute("east", "yes");
				Annotation::sub_node(s, right); });
		});
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &, FN const &) { }
};


struct Dialog::Left_floating_text : Sub_scope
{
	template <typename SCOPE, typename TEXT>
	static void view_sub_scope(SCOPE &s, TEXT const &text)
	{
		s.node("float", [&] {
			s.attribute("west", "yes");
			s.named_sub_node("label", "label", [&] {
				s.attribute("text", String<30>("  ", text));
				s.attribute("min_ex", "15"); }); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &, FN const &) { }
};


struct Dialog::Left_floating_hbox : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("float", [&] {
			s.attribute("west", "yes");
			s.named_sub_node("hbox", s.id.value, [&] {
				fn(s); }); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn)
	{
		with_narrowed_xml(at, "float", [&] (AT const &at) {
			with_narrowed_xml(at, "hbox", fn); });
	}
};


struct Dialog::Right_floating_hbox : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("float", [&] {
			s.attribute("east", "yes");
			s.named_sub_node("hbox", s.id.value, [&] {
				fn(s); }); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn)
	{
		with_narrowed_xml(at, "float", [&] (AT const &at) {
			with_narrowed_xml(at, "hbox", fn); });
	}
};


struct Dialog::Vgap : Sub_scope
{
	template <typename SCOPE>
	static void view_sub_scope(SCOPE &s)
	{
		s.node("label", [&] { s.attribute("text", " "); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &, FN const &) { }
};


struct Dialog::Centered_info_vbox : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("float", [&] {
			s.sub_node("frame", [&] {
				s.attribute("style", "unimportant");
				s.named_sub_node("vbox", s.id.value, [&] {
					fn(s); }); }); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &, FN const &) { }
};


struct Dialog::Centered_dialog_vbox : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("float", [&] {
			s.sub_node("frame", [&] {
				s.attribute("style", "important");
				s.named_sub_node("vbox", s.id.value, [&] {
					fn(s); }); }); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn)
	{
		with_narrowed_xml(at, "float", [&] (AT const &at) {
			with_narrowed_xml(at, "frame", [&] (AT const &at) {
				with_narrowed_xml(at, "vbox", fn); }); });
	}
};


struct Dialog::Titled_frame : Widget<Frame>
{
	struct Attr { unsigned min_ex; };

	template <typename FN>
	static void view(Scope<Frame> &s, Attr const attr, FN const &fn)
	{
		s.sub_node("vbox", [&] {
			if (attr.min_ex)
				s.named_sub_node("label", "min_ex", [&] {
					s.attribute("min_ex", attr.min_ex); });
			s.sub_node("label", [&] { s.attribute("text", s.id.value); });
			s.sub_node("float", [&] {
				s.sub_node("vbox", [&] () {
					fn(); }); }); });
	}

	template <typename FN>
	static void view(Scope<Frame> &s, FN const &fn)
	{
		view(s, Attr { }, fn);
	}
};


template <typename ENUM>
struct Dialog::Radio_select_button : Widget<Left_floating_hbox>
{
	ENUM const value;

	Radio_select_button(ENUM value) : value(value) { }

	template <typename TEXT>
	void view(Scope<Left_floating_hbox> &s, ENUM const &selected_value, TEXT const &text) const
	{
		bool const selected = (selected_value == value),
		           hovered  = (s.hovered() && !s.dragged() && !selected);

		s.sub_scope<Float>([&] (Scope<Left_floating_hbox, Float> &s) {
			s.sub_scope<Button>([&] (Scope<Left_floating_hbox, Float, Button> &s) {
				s.attribute("style", "radio");

				if (selected) s.attribute("selected", "yes");
				if (hovered)  s.attribute("hovered",  "yes");

				s.sub_scope<Hbox>();
			});
		});

		/* inflate vertical space to button size */
		s.sub_scope<Button>([&] (Scope<Left_floating_hbox, Button> &s) {
			s.attribute("style", "invisible");
			s.sub_scope<Label>(text); });
	}

	template <typename FN>
	void click(Clicked_at const &, FN const &fn) { fn(value); }
};


struct Dialog::Pin_button : Action_button
{
	struct Attr { bool visible; };

	void view(Scope<Button> &s, Attr attr = { .visible = true }) const
	{
		if (attr.visible) {
			bool const selected = _seq_number == s.hover.seq_number,
			           hovered  = (s.hovered() && (!s.dragged() || selected));

			if (selected) s.attribute("selected", "yes");
			if (hovered)  s.attribute("hovered",  "yes");
		} else {
			s.attribute("style", "invisible");
		}
		auto const &text = s.id.value;
		s.sub_scope<Vbox>([&] (Scope<Button, Vbox> &s) {
			s.sub_scope<Min_ex>(10);
			s.sub_scope<Vgap>();
			s.sub_scope<Dialog::Label>(text, [&] (auto &s) {
				if (!attr.visible)
					s.attribute("style", "invisible");
				s.attribute("font", "title/regular"); });
			s.sub_scope<Vgap>();
		});
	}
};


struct Dialog::Pin_row : Widget<Hbox>
{
	Hosted<Hbox, Pin_button> _buttons[3];

	template <typename S1, typename S2, typename S3>
	Pin_row(S1 const &left, S2 const &middle, S3 const &right)
	:
		_buttons { Id { left }, Id { middle }, Id { right } }
	{ }

	struct Visible { bool left, middle, right; };

	void view(Scope<Hbox> &s, Visible visible = { true, true, true }) const
	{
		s.widget(_buttons[0], Pin_button::Attr { visible.left   });
		s.widget(_buttons[1], Pin_button::Attr { visible.middle });
		s.widget(_buttons[2], Pin_button::Attr { visible.right  });
	}

	template <typename FN>
	void click(Clicked_at const &at, FN const &fn)
	{
		for (auto &button : _buttons)
			button.propagate(at, [&] { fn(button.id.value); });
	}
};


struct Dialog::Doublechecked_action_button
{
	bool selected  = false;
	bool confirmed = false;

	Hosted<Vbox, Toggle_button>          _operation;
	Hosted<Vbox, Deferred_action_button> _confirm_or_cancel;

	Doublechecked_action_button(Id::Value const &id_prefix)
	:
		_operation        (Id { Id::Value { id_prefix, " op" } }),
		_confirm_or_cancel(Id { Id::Value { id_prefix, " confirm" } })
	{ }

	void reset() { selected = false, confirmed = false; }

	template <typename TEXT>
	void view(Scope<Vbox> &s, TEXT const &text) const
	{
		s.widget(_operation, selected, [&] (Scope<Button> &s) {
			s.sub_scope<Dialog::Label>(text); });

		if (selected)
			s.widget(_confirm_or_cancel, [&] (auto &s) {
				s.template sub_scope<Dialog::Label>(confirmed ? " Cancel "
				                                              : " Confirm "); });
	}

	void click(Clicked_at const &at)
	{
		_operation.propagate(at, [&] {
			if (!confirmed)
				selected = !selected; });

		_confirm_or_cancel.propagate(at);
	}

	template <typename FN>
	void clack(Clacked_at const &at, FN const &activate_fn)
	{
		_confirm_or_cancel.propagate(at, activate_fn);
	}
};


/* local includes */
#include <view/deprecated_dialog.h>

namespace Dialog {

	template <typename FN>
	static inline void with_dummy_scope(Xml_generator &xml, FN const &fn)
	{
		static Xml_node const hover("<hover/>");
		At const no_hover(Dialog::Event::Seq_number { }, hover);
		Scope<> scope(xml, no_hover, Dialog::Event::Dragged { }, Id { });
		fn(scope);
	}
}

#endif /* _VIEW__DIALOG_H_ */
