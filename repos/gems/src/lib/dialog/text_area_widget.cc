/*
 * \brief  Text dialog
 * \author Norman Feske
 * \date   2020-01-14
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <dialog/text_area_widget.h>

using namespace Dialog;


enum {
	CODEPOINT_BACKSPACE = 8,      CODEPOINT_NEWLINE  = 10,
	CODEPOINT_UP        = 0xf700, CODEPOINT_DOWN     = 0xf701,
	CODEPOINT_LEFT      = 0xf702, CODEPOINT_RIGHT    = 0xf703,
	CODEPOINT_HOME      = 0xf729, CODEPOINT_INSERT   = 0xf727,
	CODEPOINT_DELETE    = 0xf728, CODEPOINT_END      = 0xf72b,
	CODEPOINT_PAGEUP    = 0xf72c, CODEPOINT_PAGEDOWN = 0xf72d,
};


static bool movement_codepoint(Codepoint code)
{
	auto v = code.value;
	return (v == CODEPOINT_UP)     || (v == CODEPOINT_DOWN)  ||
	       (v == CODEPOINT_LEFT)   || (v == CODEPOINT_RIGHT) ||
	       (v == CODEPOINT_HOME)   || (v == CODEPOINT_END)   ||
	       (v == CODEPOINT_PAGEUP) || (v == CODEPOINT_PAGEDOWN);
}


static bool shift_key(Input::Keycode key)
{
	return (key == Input::KEY_LEFTSHIFT) || (key == Input::KEY_RIGHTSHIFT);
}


static bool control_key(Input::Keycode key)
{
	return (key == Input::KEY_LEFTCTRL) || (key == Input::KEY_RIGHTCTRL);
}


template <typename T>
static void swap(T &v1, T &v2) { auto tmp = v1; v1 = v2; v2 = tmp; };


static unsigned unsigned_from_id(Dialog::Id const &id)
{
	unsigned value { };
	ascii_to(id.value.string(), value);
	return value;
};


void Text_area_widget::_with_position_at(::Dialog::At const &at, auto const &fn) const
{
	At::Narrowed<Vbox, Hbox, void>::with_at(at, [&] (At const &at) {

		Text::Index const y = { unsigned_from_id(at.id()) + _scroll.y.value };

		_text.apply(y, [&] (Line const &line) {
			Line::Index x = line.upper_bound();

			At::Narrowed<Float, Label, void>::with_at(at, [&] (At const &at) {
				x = { at._location.attribute_value("at", 0u) }; });

			fn(Position { x, y });
		});
	});
}


void Text_area_widget::Selection::for_each_selected_line(auto const &fn) const
{
	if (!defined())
		return;

	unsigned start_y = start->y.value, end_y = end->y.value;

	if (end_y < start_y)
		swap(start_y, end_y);

	if (end_y < start_y)
		return;

	for (unsigned i = start_y; i <= end_y; i++)
		fn(Text::Index { i }, (i == end_y));
}


void Text_area_widget::Selection::with_selection_at_line(Text::Index y, Line const &line,
                                                         auto const &fn) const
{
	if (!defined())
		return;

	Line::Index start_x = start->x, end_x = end->x;
	Text::Index start_y = start->y, end_y = end->y;

	if (end_y.value < start_y.value) {
		swap(start_x, end_x);
		swap(start_y, end_y);
	}

	if (y.value < start_y.value || y.value > end_y.value)
		return;

	if (y.value > start_y.value)
		start_x = Line::Index { 0 };

	if (y.value < end_y.value)
		end_x = line.upper_bound();

	if (start_x.value > end_x.value)
		swap(start_x, end_x);

	fn(start_x, end_x.value - start_x.value);
}


void Text_area_widget::Selection::view_selected_line(Scope<Hbox, Float, Label> &s,
                                                     Text::Index y, Line const &line) const
{
	with_selection_at_line(y, line, [&] (Line::Index const start_x, const unsigned n) {
		s.sub_node("selection", [&] () {
			s.attribute("at",     start_x.value);
			s.attribute("length", n); }); });
}


void Text_area_widget::view(Scope<Vbox> &s) const
{
	using namespace ::Dialog;

	struct Line_widget : Widget<Hbox>
	{
		struct Attr
		{
			Text::Index const  y;
			Line        const &line;
			Position    const &cursor;
			Selection   const &selection;
		};

		void view(Scope<Hbox> &s, Attr const &attr) const
		{
			bool const line_hovered = s.hovered();

			s.sub_scope<Float>([&] (Scope<Hbox, Float> &s) {
				s.attribute("north", "yes");
				s.attribute("south", "yes");
				s.attribute("west",  "yes");

				s.sub_scope<Label>(String<512>(attr.line), [&] (Scope<Hbox, Float, Label> &s) {
					s.attribute("font", "monospace/regular");
					s.attribute("hover", "yes");

					if (attr.cursor.y.value == attr.y.value)
						s.sub_node("cursor", [&] {
							s.attribute("name", "cursor");
							s.attribute("at", attr.cursor.x.value); });

					if (line_hovered) {
						unsigned const hover_x =
							s.hover._location.attribute_value("at", attr.line.upper_bound().value);

						s.sub_node("cursor", [&] {
							s.attribute("name",  "hover");
							s.attribute("style", "hover");
							s.attribute("at",    hover_x); });
					}

					attr.selection.view_selected_line(s, attr.y, attr.line);
				});
			});
		}
	};

	Dynamic_array<Line>::Range const range { .at     = _scroll.y,
	                                         .length = _max_lines };
	unsigned count = 0;
	_text.for_each(range, [&] (Text::Index const &at, Line const &line) {
		s.widget(Hosted<Vbox, Line_widget> { Id { { count } } }, Line_widget::Attr {
			.y         = at,
			.line      = line,
			.cursor    = _cursor,
			.selection = _selection
		});
		count++;
	});
}


void Text_area_widget::_delete_selection()
{
	if (!_editable)
		return;

	if (!_selection.defined())
		return;

	_modification_count++;

	/*
	 * Clear all characters within the selection
	 */

	unsigned num_lines = 0;
	Text::Index first_y { 0 };

	_selection.for_each_selected_line([&] (Text::Index const y, bool) {

		_text.apply(y, [&] (Line &line) {

			_selection.with_selection_at_line(y, line,
			                                 [&] (Line::Index x, unsigned n) {
				for (unsigned i = 0; i < n; i++) {
					line.destruct(Line::Index { x.value });

					bool const cursor_right_of_deleted_character =
						(_cursor.y.value == y.value) && (_cursor.x.value > x.value);

					if (cursor_right_of_deleted_character)
						_cursor.x.value--;
				}
			});
		});

		if (num_lines == 0)
			first_y = y;

		num_lines++;
	});

	/*
	 * Remove all selected lines, joining the remaining characters at the
	 * bounds of the selection.
	 */

	if (num_lines > 1) {

		Text::Index const next_y { first_y.value + 1 };

		while (--num_lines) {

			bool const cursor_at_deleted_line    = (_cursor.y.value == next_y.value);
			bool const cursor_below_deleted_line = (_cursor.y.value >  next_y.value);

			_text.apply(first_y, [&] (Line &first) {

				if (cursor_at_deleted_line)
					_cursor = { { first.upper_bound() },
					            { first_y } };

				_text.apply(next_y, [&] (Line &next) {
					_move_characters(next, first); });
			});

			_text.destruct(next_y);

			if (cursor_below_deleted_line)
				_cursor.y.value--;
		}
	}

	_selection.clear();
}


void Text_area_widget::_insert_printable(Codepoint code)
{
	_tie_cursor_to_end_of_line();

	_text.apply(_cursor.y, [&] (Line &line) {
		line.insert(_cursor.x, Character(code)); });

	_cursor.x.value++;
}


void Text_area_widget::_sanitize_scroll_position()
{
	/* ensure that the cursor remains visible */
	if (_cursor.y.value > 0)
		if (_scroll.y.value > _cursor.y.value - 1)
			_scroll.y.value = _cursor.y.value - 1;

	if (_cursor.y.value == 0)
		_scroll.y.value = 0;

	if (_scroll.y.value + _max_lines < _cursor.y.value + 2)
		_scroll.y.value = _cursor.y.value - _max_lines + 2;

	_clamp_scroll_position_to_upper_bound();
}


void Text_area_widget::_handle_printable(Codepoint code)
{
	if (!_editable)
		return;

	_modification_count++;

	_delete_selection();
	_insert_printable(code);
}


void Text_area_widget::_move_characters(Line &from, Line &to)
{
	/* move all characters of line 'from' to the end of line 'to' */
	Line::Index const first { 0 };
	while (from.exists(first)) {
		from.apply(first, [&] (Codepoint &code) {
			to.append(code); });
		from.destruct(first);
	}
}


void Text_area_widget::_handle_backspace()
{
	if (!_editable)
		return;

	_modification_count++;

	/* eat backspace when deleting a selection */
	if (_selection.defined()) {
		_delete_selection();
		return;
	}

	if (_cursor.x.value > 0) {
		_cursor.x.value--;

		_text.apply(_cursor.y, [&] (Line &line) {
			line.destruct(_cursor.x); });

		return;
	}

	if (_cursor.y.value == 0)
		return;

	/* join line with previous line */
	Text::Index const prev_y { _cursor.y.value - 1 };

	_text.apply(prev_y, [&] (Line &prev_line) {

		_cursor.x = prev_line.upper_bound();

		_text.apply(_cursor.y, [&] (Line &line) {
			_move_characters(line, prev_line); });
	});

	_text.destruct(_cursor.y);

	_cursor.y = prev_y;
}


void Text_area_widget::_handle_delete()
{
	if (!_editable)
		return;

	_modification_count++;

	/* eat delete when deleting a selection */
	if (_selection.defined()) {
		_delete_selection();
		return;
	}

	if (_end_of_text())
		return;

	_handle_right();
	_handle_backspace();
}


void Text_area_widget::_handle_newline()
{
	if (!_editable)
		return;

	_modification_count++;

	_delete_selection();

	/* create new line at cursor position */
	Text::Index const new_y { _cursor.y.value + 1 };
	_text.insert(new_y, _alloc);

	/* take the characters after the cursor to the new line */
	_text.apply(_cursor.y, [&] (Line &line) {
		_text.apply(new_y, [&] (Line &new_line) {
			while (line.exists(_cursor.x)) {
				line.apply(_cursor.x, [&] (Codepoint code) {
					new_line.append(code); });
				line.destruct(_cursor.x);
			}
		});
	});

	_cursor.y = new_y;
	_cursor.x.value = 0;
}


void Text_area_widget::_handle_left()
{
	_tie_cursor_to_end_of_line();

	if (_cursor.x.value == 0) {
		if (_cursor.y.value > 0) {
			_cursor.y.value--;
			_text.apply(_cursor.y, [&] (Line &line) {
				_cursor.x = line.upper_bound(); });
		}
	} else {
		_cursor.x.value--;
	}
}


void Text_area_widget::_handle_right()
{
	if (!_cursor_at_end_of_line()) {
		_cursor.x.value++;
		return;
	}

	if (!_cursor_at_last_line()) {
		_cursor.x.value = 0;
		_cursor.y.value++;
	}
}


void Text_area_widget::_handle_up()
{
	if (_cursor.y.value > 0)
		_cursor.y.value--;
}


void Text_area_widget::_handle_down()
{
	if (_cursor.y.value + 1 < _text.upper_bound().value)
		_cursor.y.value++;
}


void Text_area_widget::_handle_pageup()
{
	if (_max_lines != ~0U) {
		for (unsigned i = 0; i < _max_lines; i++)
			_handle_up();
	} else {
		_cursor.y.value = 0;
	}
}


void Text_area_widget::_handle_pagedown()
{
	if (_max_lines != ~0U) {
		for (unsigned i = 0; i < _max_lines; i++)
			_handle_down();
	} else {
		_cursor.y.value = _text.upper_bound().value;
	}
}


void Text_area_widget::_handle_home()
{
	_cursor.x.value = 0;
}


void Text_area_widget::_handle_end()
{
	_text.apply(_cursor.y, [&] (Line &line) {
		_cursor.x = line.upper_bound(); });
}


void Text_area_widget::click(Clicked_at const &at)
{
	_with_position_at(at, [&] (Position const pos) {

		if (_shift) {
			_selection.end.construct(pos);
		} else {
			_selection.start.construct(pos);
			_selection.end.destruct();
		}

		_drag = true;
	});
}


void Text_area_widget::clack(Clacked_at const &at, Action &action)
{
	_with_position_at(at, [&] (Position const pos) {
		_cursor = pos; });

	_drag = false;

	if (_selection.defined())
		action.trigger_copy();
}


void Text_area_widget::drag(Dragged_at const &at)
{
	_with_position_at(at, [&] (Position const pos) {
		_selection.end.construct(pos); });
}


void Text_area_widget::handle_event(Event const &event, Action &action)
{
	bool update_dialog = false;

	event.event.handle_press([&] (Input::Keycode key, Codepoint code) {

		bool key_has_visible_effect = true;

		if (shift_key(key)) {
			_shift = true;
			if (!_selection.defined()) {
				_selection.start.construct(_cursor);
				_selection.end.destruct();
			}
		}

		if (control_key(key))
			_control = true;

		if (!_control) {

			if (!_shift && movement_codepoint(code))
				_selection.clear();

			if (_printable(code)) {
				_handle_printable(code);
			}
			else if (code.value == CODEPOINT_BACKSPACE) { _handle_backspace(); }
			else if (code.value == CODEPOINT_DELETE)    { _handle_delete(); }
			else if (code.value == CODEPOINT_NEWLINE)   { _handle_newline(); }
			else if (code.value == CODEPOINT_LEFT)      { _handle_left(); }
			else if (code.value == CODEPOINT_UP)        { _handle_up(); }
			else if (code.value == CODEPOINT_DOWN)      { _handle_down(); }
			else if (code.value == CODEPOINT_RIGHT)     { _handle_right(); }
			else if (code.value == CODEPOINT_PAGEDOWN)  { _handle_pagedown(); }
			else if (code.value == CODEPOINT_PAGEUP)    { _handle_pageup(); }
			else if (code.value == CODEPOINT_HOME)      { _handle_home(); }
			else if (code.value == CODEPOINT_END)       { _handle_end(); }
			else if (code.value == CODEPOINT_INSERT)    { action.trigger_paste(); }
			else {
				key_has_visible_effect = false;
			}

			if (_shift && movement_codepoint(code))
				_selection.end.construct(_cursor);
		}

		if (_control) {

			if (code.value == 'c')
				action.trigger_copy();

			if (code.value == 'x') {
				action.trigger_copy();
				_delete_selection();
			}

			if (code.value == 'v')
				action.trigger_paste();

			if (code.value == 's')
				action.trigger_save();
		}

		if (key_has_visible_effect)
			update_dialog = true;
	});

	event.event.handle_release([&] (Input::Keycode key) {
		if (shift_key(key))   _shift   = false;
		if (control_key(key)) _control = false;
	});

	bool const all_lines_visible =
		(_max_lines == ~0U) || (_text.upper_bound().value <= _max_lines);

	if (!all_lines_visible) {
		event.event.handle_wheel([&] (int, int y) {

			/* scroll at granulatory of 1/5th of vertical view size */
			y *= max(1U, _max_lines / 5);

			if (y < 0)
				_scroll.y.value += -y;

			if (y > 0)
				_scroll.y.value -= min((int)_scroll.y.value, y);

			update_dialog = true;
		});
	}

	/* adjust scroll position */
	if (all_lines_visible)
		_scroll.y.value = 0;

	if (event.event.press() && !event.event.key_press(Input::BTN_LEFT))
		_sanitize_scroll_position();

	if (update_dialog)
		action.refresh_text_area();
}


void Text_area_widget::move_cursor_to(::Dialog::At const &at)
{
	_with_position_at(at, [&] (Position pos) {
		_cursor = pos; });

	_sanitize_scroll_position();
}


void Text_area_widget::clear()
{
	Text::Index const first { 0 };

	while (_text.exists(first))
		_text.destruct(first);

	_cursor.x.value = 0;
	_cursor.y.value = 0;
}


void Text_area_widget::append_character(Codepoint c)
{
	if (_printable(c)) {
		Text::Index const y { _text.upper_bound().value - 1 };
		_text.apply(y, [&] (Line &line) {
			line.append(c); });
	}
}


void Text_area_widget::insert_at_cursor_position(Codepoint c)
{
	if (_printable(c)) {
		_insert_printable(c);
		_modification_count++;
		return;
	}

	if (c.value == CODEPOINT_NEWLINE)
		_handle_newline();
}


void Text_area_widget::gen_clipboard_content(Xml_generator &xml) const
{
	if (!_selection.defined())
		return;

	auto for_each_selected_character = [&] (auto fn)
	{
		_selection.for_each_selected_line([&] (Text::Index const y, bool const last) {
			_text.apply(y, [&] (Line const &line) {
				_selection.with_selection_at_line(y, line, [&] (Line::Index x, unsigned n) {
					for (unsigned i = 0; i < n; i++)
						line.apply(Line::Index { x.value + i }, [&] (Codepoint c) {
							fn(c); }); }); });

			if (!last)
				fn(Codepoint{'\n'});
		});
	};

	for_each_selected_character([&] (Codepoint c) {
		String<10> const utf8(c);
		if (utf8.valid())
			xml.append_sanitized(utf8.string(), utf8.length() - 1);
	});
}
