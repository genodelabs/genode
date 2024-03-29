/*
 * \brief  Text dialog
 * \author Norman Feske
 * \date   2020-01-14
 */

/*
 * Copyright (C) 2020-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DIALOG__TEXT_AREA_WIDGET_H_
#define _INCLUDE__DIALOG__TEXT_AREA_WIDGET_H_

/* Genode includes */
#include <util/reconstructible.h>
#include <util/utf8.h>
#include <dialog/widgets.h>
#include <gems/dynamic_array.h>

namespace Dialog { struct Text_area_widget; }


struct Dialog::Text_area_widget : Widget<Vbox>
{
	public:

		struct Action : Interface, Noncopyable
		{
			virtual void trigger_copy() = 0;
			virtual void trigger_paste() = 0;
			virtual void trigger_save() = 0;
			virtual void refresh_text_area() = 0;
		};

	private:

		Allocator &_alloc;

		struct Character : Codepoint
		{
			Character(Codepoint &codepoint) : Codepoint(codepoint) { }

			void print(Output &out) const
			{
				if (value == '"')
					Genode::print(out, "&quot;");
				else if (value == 9)
					Genode::print(out, " ");
				else
					Codepoint::print(out);
			}
		};

		using Line = Dynamic_array<Character>;
		using Text = Dynamic_array<Line>;

		Text _text { _alloc };

		struct Position
		{
			Line::Index x;
			Text::Index y;

			bool operator != (Position const &other) const
			{
				return (x.value != other.x.value) || (y.value != other.y.value);
			}
		};

		Position _cursor { { 0 }, { 0 } };

		Position _scroll { { 0 }, { 0 } };

		Constructible<Position> _hovered_position { };

		unsigned _max_lines = ~0U;

		bool _editable = false;

		unsigned _modification_count = 0;

		struct Selection
		{
			Constructible<Position> start;
			Constructible<Position> end;

			void clear()
			{
				start.destruct();
				end  .destruct();
			}

			bool defined() const
			{
				return start.constructed() && end.constructed() && (*start != *end);
			}

			template <typename FN>
			void for_each_selected_line(FN const &) const;

			template <typename FN>
			void with_selection_at_line(Text::Index y, Line const &, FN const &) const;

			/* generate dialog model */
			void view_selected_line(Scope<Hbox, Float, Label> &, Text::Index, Line const &) const;
		};

		bool _drag    = false;
		bool _shift   = false;
		bool _control = false;

		Selection _selection { };

		static bool _printable(Codepoint code)
		{
			if (!code.valid())
				return false;

			if (code.value == '\t')
				return true;

			return (code.value >= 0x20 && code.value < 0xf000);
		}

		bool _cursor_at_last_line() const
		{
			return (_cursor.y.value + 1 >= _text.upper_bound().value);
		}

		bool _cursor_at_end_of_line() const
		{
			bool result = false;
			_text.apply(_cursor.y, [&] (Line &line) {
				result = (_cursor.x.value >= line.upper_bound().value); });
			return result;
		}

		void _tie_cursor_to_end_of_line()
		{
			_text.apply(_cursor.y, [&] (Line &line) {
				if (_cursor.x.value > line.upper_bound().value)
					_cursor.x = line.upper_bound(); });
		}

		bool _end_of_text() const
		{
			return _cursor_at_last_line() && _cursor_at_end_of_line();
		}

		void _clamp_scroll_position_to_upper_bound()
		{
			if (_max_lines != ~0U)
				if (_scroll.y.value + _max_lines > _text.upper_bound().value)
					_scroll.y.value = max(_text.upper_bound().value, _max_lines)
					                - _max_lines;
		}

		void _sanitize_scroll_position();
		void _move_characters(Line &, Line &);
		void _delete_selection();
		void _insert_printable(Codepoint);
		void _handle_printable(Codepoint);
		void _handle_backspace();
		void _handle_delete();
		void _handle_newline();
		void _handle_left();
		void _handle_right();
		void _handle_up();
		void _handle_down();
		void _handle_pageup();
		void _handle_pagedown();
		void _handle_home();
		void _handle_end();

		void _with_position_at(At const &, auto const &) const;

	public:

		Text_area_widget(Allocator &alloc) : _alloc(alloc) { clear(); }

		void view(Scope<Vbox> &) const;

		void click(Clicked_at const &);
		void clack(Clacked_at const &, Action &);
		void drag (Dragged_at const &);

		void editable(bool editable) { _editable = editable; }

		unsigned modification_count() const { return _modification_count; }

		void max_lines(unsigned max_lines) { _max_lines = max_lines; }

		void handle_event(Event const &, Action &);

		void move_cursor_to(::Dialog::At const &);

		void clear();

		void append_newline() { _text.append(_alloc); }

		void append_character(Codepoint c);

		/* insert character and advance cursor */
		void insert_at_cursor_position(Codepoint);

		void gen_clipboard_content(Xml_generator &xml) const;

		template <typename FN>
		void for_each_character(FN const &fn) const
		{
			_text.for_each([&] (Text::Index at, Line const &line) {
				line.for_each([&] (Line::Index, Character c) {
					fn(c); });

				if (at.value + 1 < _text.upper_bound().value)
					fn(Codepoint{'\n'});
			});
		}
};

#endif /* _INCLUDE__DIALOG__TEXT_AREA_WIDGET_H_ */
