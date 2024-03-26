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
	struct Top_left_floating_hbox;
	struct Right_floating_hbox;
	struct Vgap;
	struct Small_vgap;
	struct Button_vgap;
	struct Centered_info_vbox;
	struct Centered_dialog_vbox;
	struct Icon;
	struct Titled_frame;
	struct Pin_button;
	struct Pin_row;
	struct Menu_entry;
	struct Operation_button;
	struct Right_floating_off_on;
	struct Doublechecked_action_button;
	template <typename> struct Radio_select_button;
	template <typename> struct Choice;
}


struct Dialog::Annotation : Sub_scope
{
	static void sub_node(auto &scope, auto const &text)
	{
		scope.sub_node("label", [&] {
			scope.attribute("text", text);
			scope.attribute("font", "annotation/regular"); });
	}

	static void view_sub_scope(auto &scope, auto const &text)
	{
		sub_node(scope, text);
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};


struct Dialog::Left_annotation : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &text)
	{
		s.node("hbox", [&] {
			s.sub_node("float", [&] {
				s.attribute("west", "yes");
				Annotation::sub_node(s, text); }); });
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};


struct Dialog::Left_right_annotation : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &left, auto const &right)
	{
		s.node("hbox", [&] {
			s.named_sub_node("float", "left", [&] {
				s.attribute("west", "yes");
				Annotation::sub_node(s, left); });

			s.named_sub_node("float", "right", [&] {
				s.attribute("east", "yes");
				Annotation::sub_node(s, right); });
		});
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};


struct Dialog::Left_floating_text : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &text)
	{
		s.node("float", [&] {
			s.attribute("west", "yes");
			s.named_sub_node("label", "label", [&] {
				s.attribute("text", String<30>("  ", text));
				s.attribute("min_ex", "15"); }); });
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};


struct Dialog::Left_floating_hbox : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &fn)
	{
		s.node("float", [&] {
			s.attribute("west", "yes");
			s.named_sub_node("hbox", s.id.value, [&] {
				fn(s); }); });
	}

	static void with_narrowed_at(auto const &at, auto const &fn)
	{
		with_narrowed_xml(at, "float", [&] (auto const &at) {
			with_narrowed_xml(at, "hbox", fn); });
	}
};


struct Dialog::Top_left_floating_hbox : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &fn)
	{
		s.node("float", [&] {
			s.attribute("west",  "yes");
			s.attribute("north", "yes");
			s.named_sub_node("hbox", s.id.value, [&] {
				fn(s); }); });
	}

	static void with_narrowed_at(auto const &at, auto const &fn)
	{
		with_narrowed_xml(at, "float", [&] (auto const &at) {
			with_narrowed_xml(at, "hbox", fn); });
	}
};


struct Dialog::Right_floating_hbox : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &fn)
	{
		s.node("float", [&] {
			s.attribute("east", "yes");
			s.named_sub_node("hbox", s.id.value, [&] {
				fn(s); }); });
	}

	static void with_narrowed_at(auto const &at, auto const &fn)
	{
		with_narrowed_xml(at, "float", [&] (auto const &at) {
			with_narrowed_xml(at, "hbox", fn); });
	}
};


struct Dialog::Vgap : Sub_scope
{
	static void view_sub_scope(auto &s)
	{
		s.node("label", [&] { s.attribute("text", " "); });
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};


struct Dialog::Small_vgap : Sub_scope
{
	static void view_sub_scope(auto &s)
	{
		s.node("label", [&] {
			s.attribute("text", "");
			s.attribute("font", "annotation/regular"); });
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};


struct Dialog::Button_vgap : Sub_scope
{
	static void view_sub_scope(auto &s)
	{
		/* inflate vertical space to button size */
		s.node("button", [&] {
			s.attribute("style", "invisible");
			s.sub_node("label", [&] { s.attribute("text", ""); }); });
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};


struct Dialog::Centered_info_vbox : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &fn)
	{
		s.node("float", [&] {
			s.sub_node("frame", [&] {
				s.attribute("style", "unimportant");
				s.named_sub_node("vbox", s.id.value, [&] {
					fn(s); }); }); });
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};


struct Dialog::Centered_dialog_vbox : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &fn)
	{
		s.node("float", [&] {
			s.sub_node("frame", [&] {
				s.attribute("style", "important");
				s.named_sub_node("vbox", s.id.value, [&] {
					fn(s); }); }); });
	}

	static void with_narrowed_at(auto const &at, auto const &fn)
	{
		with_narrowed_xml(at, "float", [&] (auto const &at) {
			with_narrowed_xml(at, "frame", [&] (auto const &at) {
				with_narrowed_xml(at, "vbox", fn); }); });
	}
};


struct Dialog::Icon : Sub_scope
{
	struct Attr { bool hovered, selected; };

	/* used whenever the icon's hover sensitivity is larger than the icon */
	static void view_sub_scope(auto &s, auto const &style, Attr attr)
	{
		s.node("float", [&] {
			s.sub_node("button", [&] {
				s.attribute("style", style);
				if (attr.selected) s.attribute("selected", "yes");
				if (attr.hovered)  s.attribute("hovered",  "yes");
				s.sub_node("hbox", [&] { }); }); });
	}

	/* used when hovering responds only to the icon's boundaries */
	static void view_sub_scope(auto &s, auto const &style, bool selected)
	{
		view_sub_scope(s, style, Attr { .hovered  = s.hovered(),
		                                .selected = selected });
	}

	static void with_narrowed_at(auto const &at, auto const &fn)
	{
		with_narrowed_xml(at, "float", [&] (auto const &at) {
			with_narrowed_xml(at, "button", fn); });
	}
};


struct Dialog::Titled_frame : Widget<Frame>
{
	struct Attr { unsigned min_ex; };

	static void view(Scope<Frame> &s, Attr const attr, auto const &fn)
	{
		s.sub_node("vbox", [&] {
			if (attr.min_ex)
				s.named_sub_node("label", "min_ex", [&] {
					s.attribute("min_ex", attr.min_ex); });
			s.sub_node("label", [&] { s.attribute("text", s.id.value); });
			s.sub_node("float", [&] {
				s.sub_node("vbox", [&] {
					fn(); }); }); });
	}

	static void view(Scope<Frame> &s, auto const &fn)
	{
		view(s, Attr { }, fn);
	}
};


template <typename ENUM>
struct Dialog::Radio_select_button : Widget<Left_floating_hbox>
{
	ENUM const value;

	Radio_select_button(ENUM value) : value(value) { }

	void view(Scope<Left_floating_hbox> &s, ENUM const &selected_value, auto const &text) const
	{
		bool const selected = (selected_value == value),
		           hovered  = (s.hovered() && !s.dragged() && !selected);

		s.sub_scope<Icon>("radio", Icon::Attr { .hovered  = hovered,
		                                        .selected = selected });
		s.sub_scope<Label>(String<100>(" ", text));
		s.sub_scope<Button_vgap>();
	}

	void view(Scope<Left_floating_hbox> &s, ENUM const &selected_value) const
	{
		view(s, selected_value, s.id.value);
	}

	void click(Clicked_at const &, auto const &fn) const { fn(value); }
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
			s.sub_scope<Label>(text, [&] (auto &s) {
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

	Pin_row(auto const &left, auto const &middle, auto const &right)
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

	void click(Clicked_at const &at, auto const &fn)
	{
		for (auto &button : _buttons)
			button.propagate(at, [&] { fn(button.id.value); });
	}
};


struct Dialog::Menu_entry : Widget<Left_floating_hbox>
{
	void view(Scope<Left_floating_hbox> &s, bool selected, auto const &text,
	          char const * const style = "radio") const
	{
		bool const hovered = (s.hovered() && !s.dragged());

		s.sub_scope<Icon>(style, Icon::Attr { .hovered  = hovered,
		                                      .selected = selected });
		s.sub_scope<Label>(String<100>(" ", text));
		s.sub_scope<Button_vgap>();
	}

	void click(Clicked_at const &, auto const &fn) const { fn(); }
};


struct Dialog::Operation_button : Widget<Button>
{
	void view(Scope<Button> &s, bool selected, auto const &text) const
	{
		if (selected) {
			s.attribute("selected", "yes");
			s.attribute("style", "unimportant");
		}

		if (s.hovered() && !s.dragged() && !selected)
			s.attribute("hovered",  "yes");

		s.sub_scope<Label>(String<50>("  ", text, "  "));
	}

	void view(Scope<Button> &s, bool selected) const
	{
		view(s, selected, s.id.value);
	}

	void click(Clicked_at const &, auto const &fn) const { fn(); }
};


struct Dialog::Right_floating_off_on : Widget<Right_floating_hbox>
{
	struct Attr { bool on, transient; };

	Hosted<Right_floating_hbox, Select_button<bool>> const
		_off { Id { "  Off  " }, false },
		_on  { Id { "  On  "  }, true  };

	void view(Scope<Right_floating_hbox> &s, Attr attr) const
	{
		auto transient_attr_fn = [&] (Scope<Button> &s)
		{
			if (attr.transient)
				s.attribute("style", "unimportant");

			s.sub_scope<Label>(s.id.value);
		};

		s.widget(_off, attr.on, transient_attr_fn);
		s.widget(_on,  attr.on, transient_attr_fn);
	}

	void view(Scope<Right_floating_hbox> &s, bool on) const
	{
		view(s, Attr { .on = on, .transient = false });
	}

	void click(Clicked_at const &at, auto const &fn) const
	{
		_off.propagate(at, [&] (bool) { fn(false); });
		_on .propagate(at, [&] (bool) { fn(true);  });
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

	void view(Scope<Vbox> &s, auto const &text) const
	{
		s.widget(_operation, selected, [&] (Scope<Button> &s) {
			s.sub_scope<Label>(text); });

		if (selected)
			s.widget(_confirm_or_cancel, [&] (auto &s) {
				s.template sub_scope<Label>(confirmed ? " Cancel "
				                                      : " Confirm "); });
	}

	void click(Clicked_at const &at)
	{
		_operation.propagate(at, [&] {
			if (!confirmed)
				selected = !selected; });

		_confirm_or_cancel.propagate(at);
	}

	void clack(Clacked_at const &at, auto const &activate_fn)
	{
		_confirm_or_cancel.propagate(at, activate_fn);
	}
};


template <typename ENUM>
struct Dialog::Choice : Widget<Hbox>
{
	ENUM const _unfold_value;

	Choice(ENUM const unfold_value) : _unfold_value(unfold_value) { }

	struct Attr
	{
		unsigned left_ex, right_ex;
		ENUM     unfolded;
		Id       selected_item;
	};

	struct Sub_scope
	{
		Scope<> &_scope;

		bool const _unfolded;
		Id   const _selected_item;

		void widget(auto const &hosted, auto &&... args)
		{
			if (_unfolded || (hosted.id == _selected_item))
				hosted._view_hosted(_scope, args...);
		}
	};

	void view(Scope<Hbox> &s, Attr attr, auto const &fn) const
	{
		Id::Value const text     = s.id.value;
		bool      const unfolded = (attr.unfolded == _unfold_value);

		s.sub_scope<Vbox>([&] (Scope<Hbox, Vbox> &s) {
			s.sub_scope<Min_ex>(attr.left_ex);
			s.sub_scope<Float>([&] (Scope<Hbox, Vbox, Float> &s) {
				s.attribute("north", true);
				s.attribute("west",  true);
				s.sub_scope<Frame>([&] (Scope<Hbox, Vbox, Float, Frame> &s) {
					s.attribute("style", "invisible");
					s.sub_scope<Hbox>([&] (Scope<Hbox, Vbox, Float, Frame, Hbox> &s) {
						s.sub_scope<Label>(text);
						s.sub_scope<Button_vgap>(); }); }); }); });

		s.sub_scope<Frame>([&] (Scope<Hbox, Frame> &s) {
			s.sub_scope<Vbox>([&] (Scope<Hbox, Frame, Vbox> &s) {
				s.sub_scope<Min_ex>(attr.right_ex);
				s.as_new_scope([&] (Scope<> &s) {
					Sub_scope sub_scope { s, unfolded, attr.selected_item };
					fn(sub_scope); }); }); });
	}

	void click(Clicked_at const &at, ENUM &unfolded,
	           auto const &fold_all_fn, auto const &item_fn) const
	{
		if (unfolded != _unfold_value) {
			unfolded = _unfold_value;
			return;
		}

		bool clicked_at_item = false;
		Hbox::with_narrowed_at(at, [&] (Clicked_at const &at) {
			Frame::with_narrowed_at(at, [&] (Clicked_at const &at) {
				Vbox::with_narrowed_at(at, [&] (Clicked_at const &at) {
					clicked_at_item = true;
					item_fn(at); }); }); });

		if (!clicked_at_item)
			fold_all_fn();
	}
};

#endif /* _VIEW__DIALOG_H_ */
