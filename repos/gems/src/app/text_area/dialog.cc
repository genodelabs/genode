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

/* local includes */
#include <dialog.h>

using namespace Text_area;


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


template <typename FN>
void Dialog::Selection::for_each_selected_line(FN const &fn) const
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


template <typename FN>
void Dialog::Selection::with_selection_at_line(Text::Index y, Line const &line,
                                               FN const &fn) const
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


void Dialog::Selection::gen_selected_line(Xml_generator &xml,
                                          Text::Index y, Line const &line) const
{
	with_selection_at_line(y, line, [&] (Line::Index const start_x, const unsigned n) {
		xml.node("selection", [&] () {
			xml.attribute("at",     start_x.value);
			xml.attribute("length", n);
		});
	});
}


void Dialog::produce_xml(Xml_generator &xml)
{
	auto gen_line = [&] (Text::Index at, Line const &line)
	{
		xml.node("hbox", [&] () {
			xml.attribute("name", at.value - _scroll.y.value);
			xml.node("float", [&] () {
				xml.attribute("north", "yes");
				xml.attribute("south", "yes");
				xml.attribute("west",  "yes");
				xml.node("label", [&] () {
					xml.attribute("font", "monospace/regular");
					xml.attribute("text", String<512>(line));

					if (_cursor.y.value == at.value)
						xml.node("cursor", [&] () {
							xml.attribute("name", "cursor");
							xml.attribute("at", _cursor.x.value); });

					if (_hovered_position.constructed())
						if (_hovered_position->y.value == at.value)
							xml.node("cursor", [&] () {
								xml.attribute("name", "hover");
								xml.attribute("style", "hover");
								xml.attribute("at", _hovered_position->x.value); });

					_selection.gen_selected_line(xml, at, line);
				});
			});
		});
	};

	xml.node("frame", [&] () {
		xml.node("button", [&] () {
			xml.attribute("name", "text");

			if (_text_hovered)
				xml.attribute("hovered", "yes");

			xml.node("float", [&] () {
				xml.attribute("north", "yes");
				xml.attribute("east",  "yes");
				xml.attribute("west",  "yes");
				xml.node("vbox", [&] () {
					Dynamic_array<Line>::Range const range { .at     = _scroll.y,
					                                         .length = _max_lines };
					_text.for_each(range, gen_line);
				});
			});
		});
	});
}


void Dialog::_delete_selection()
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
					_cursor = { .x = first.upper_bound(),
					            .y = first_y };

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


void Dialog::_insert_printable(Codepoint code)
{
	_tie_cursor_to_end_of_line();

	_text.apply(_cursor.y, [&] (Line &line) {
		line.insert(_cursor.x, Character(code)); });

	_cursor.x.value++;
}


void Dialog::_handle_printable(Codepoint code)
{
	if (!_editable)
		return;

	_modification_count++;

	_delete_selection();
	_insert_printable(code);
}


void Dialog::_move_characters(Line &from, Line &to)
{
	/* move all characters of line 'from' to the end of line 'to' */
	Line::Index const first { 0 };
	while (from.exists(first)) {
		from.apply(first, [&] (Codepoint &code) {
			to.append(code); });
		from.destruct(first);
	}
}


void Dialog::_handle_backspace()
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


void Dialog::_handle_delete()
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


void Dialog::_handle_newline()
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


void Dialog::_handle_left()
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


void Dialog::_handle_right()
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


void Dialog::_handle_up()
{
	if (_cursor.y.value > 0)
		_cursor.y.value--;
}


void Dialog::_handle_down()
{
	if (_cursor.y.value + 1 < _text.upper_bound().value)
		_cursor.y.value++;
}


void Dialog::_handle_pageup()
{
	if (_max_lines != ~0U) {
		for (unsigned i = 0; i < _max_lines; i++)
			_handle_up();
	} else {
		_cursor.y.value = 0;
	}
}


void Dialog::_handle_pagedown()
{
	if (_max_lines != ~0U) {
		for (unsigned i = 0; i < _max_lines; i++)
			_handle_down();
	} else {
		_cursor.y.value = _text.upper_bound().value;
	}
}


void Dialog::_handle_home()
{
	_cursor.x.value = 0;
}


void Dialog::_handle_end()
{
	_text.apply(_cursor.y, [&] (Line &line) {
		_cursor.x = line.upper_bound(); });
}


void Dialog::handle_input_event(Input::Event const &event)
{
	bool update_dialog = false;

	Position const orig_cursor = _cursor;

	auto cursor_to_hovered_position = [&] ()
	{
		if (_hovered_position.constructed()) {
			_cursor.x = _hovered_position->x;
			_cursor.y = _hovered_position->y;
			update_dialog = true;
		}
	};

	event.handle_press([&] (Input::Keycode key, Codepoint code) {

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
			else if (code.value == CODEPOINT_INSERT)    { _trigger_paste.trigger_paste(); }
			else {
				key_has_visible_effect = false;
			}

			if (_shift && movement_codepoint(code))
				_selection.end.construct(_cursor);
		}

		if (_control) {

			if (code.value == 'c')
				_trigger_copy.trigger_copy();

			if (code.value == 'x') {
				_trigger_copy.trigger_copy();
				_delete_selection();
			}

			if (code.value == 'v')
				_trigger_paste.trigger_paste();

			if (code.value == 's')
				_trigger_save.trigger_save();
		}

		if (key_has_visible_effect)
			update_dialog = true;

		bool const click = (key == Input::BTN_LEFT);
		if (click && _hovered_position.constructed()) {

			if (_shift)
				_selection.end.construct(*_hovered_position);
			else
				_selection.start.construct(*_hovered_position);

			_drag = true;
		}

		bool const middle_click = (key == Input::BTN_MIDDLE);
		if (middle_click) {
			cursor_to_hovered_position();
			_trigger_paste.trigger_paste();
		}
	});

	if (_drag && _hovered_position.constructed()) {
		_selection.end.construct(*_hovered_position);
		update_dialog = true;
	}

	bool const clack = event.key_release(Input::BTN_LEFT);
	if (clack) {
		cursor_to_hovered_position();
		_drag = false;

		if (_selection.defined())
			_trigger_copy.trigger_copy();
	}

	event.handle_release([&] (Input::Keycode key) {
		if (shift_key(key))   _shift   = false;
		if (control_key(key)) _control = false;
	});

	bool const all_lines_visible =
		(_max_lines == ~0U) || (_text.upper_bound().value <= _max_lines);

	if (!all_lines_visible) {
		event.handle_wheel([&] (int, int y) {

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
	if (all_lines_visible) {
		_scroll.y.value = 0;

	} else if (orig_cursor != _cursor) {

		/* ensure that the cursor remains visible */
		if (_cursor.y.value > 0)
			if (_scroll.y.value > _cursor.y.value - 1)
				_scroll.y.value = _cursor.y.value - 1;

		if (_cursor.y.value == 0)
			_scroll.y.value = 0;

		if (_scroll.y.value + _max_lines < _cursor.y.value + 2)
			_scroll.y.value = _cursor.y.value - _max_lines + 2;
	}

	_clamp_scroll_position_to_upper_bound();

	if (update_dialog)
		rom_session.trigger_update();
}


void Dialog::handle_hover(Xml_node const &hover)
{
	Constructible<Position> orig_pos { };

	if (_hovered_position.constructed())
		orig_pos.construct(*_hovered_position);

	_hovered_position.destruct();

	auto with_hovered_line = [&] (Xml_node node)
	{
		Text::Index const y {
			node.attribute_value("name", _text.upper_bound().value)
			+ _scroll.y.value };

		_text.apply(y, [&] (Line const &line) {

			Line::Index const max_x = line.upper_bound();

			_hovered_position.construct(max_x, y);

			node.with_sub_node("float", [&] (Xml_node node) {
				node.with_sub_node("label", [&] (Xml_node node) {

					Line::Index const x {
						node.attribute_value("at", max_x.value) };

					_hovered_position.construct(x, y);
				});
			});
		});
	};

	bool const hover_changed =
		(orig_pos.constructed() != _hovered_position.constructed());

	bool const position_changed = orig_pos.constructed()
	                           && _hovered_position.constructed()
	                           && (*orig_pos != *_hovered_position);

	bool const orig_text_hovered = _text_hovered;

	_text_hovered = false;

	hover.with_sub_node("frame", [&] (Xml_node node) {
		node.with_sub_node("button", [&] (Xml_node node) {

			_text_hovered = true;

			node.with_sub_node("float", [&] (Xml_node node) {
				node.with_sub_node("vbox", [&] (Xml_node node) {
					node.with_sub_node("hbox", [&] (Xml_node node) {
						with_hovered_line(node); }); }); }); }); });

	if (hover_changed || position_changed || (_text_hovered != orig_text_hovered))
		rom_session.trigger_update();
}


Dialog::Dialog(Entrypoint &ep, Ram_allocator &ram, Region_map &rm,
               Allocator &alloc, Trigger_copy &trigger_copy,
               Trigger_paste &trigger_paste, Trigger_save &trigger_save)
:
	Xml_producer("dialog"),
	rom_session(ep, ram, rm, *this),
	_alloc(alloc),
	_trigger_copy(trigger_copy),
	_trigger_paste(trigger_paste),
	_trigger_save(trigger_save)
{
	clear();
}


void Dialog::clear()
{
	Text::Index const first { 0 };

	while (_text.exists(first))
		_text.destruct(first);

	_cursor.x.value = 0;
	_cursor.y.value = 0;
}


void Dialog::insert_at_cursor_position(Codepoint c)
{
	if (_printable(c)) {
		_insert_printable(c);
		_modification_count++;
		return;
	}

	if (c.value == CODEPOINT_NEWLINE)
		_handle_newline();
}


void Dialog::gen_clipboard_content(Xml_generator &xml) const
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
