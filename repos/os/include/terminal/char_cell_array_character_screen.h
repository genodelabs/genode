/*
 * \brief  Char-cell-array-based implementation of a character screen
 * \author Norman Feske
 * \date   2013-01-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__CHAR_CELL_ARRAY_CHARACTER_SCREEN_H_
#define _TERMINAL__CHAR_CELL_ARRAY_CHARACTER_SCREEN_H_

/* terminal includes */
#include <terminal/cell_array.h>
#include <terminal/font_face.h>

struct Char_cell
{
	unsigned char attr;
	unsigned char ascii;
	unsigned char color;

	enum { ATTR_COLIDX_MASK = 0x07,
	       ATTR_CURSOR      = 0x10,
	       ATTR_INVERSE     = 0x20,
	       ATTR_HIGHLIGHT   = 0x40 };

	enum { COLOR_MASK = 0x3f }; /* 111111 */

	Char_cell() : attr(0), ascii(0), color(0) { }

	Char_cell(unsigned char c, Font_face f,
	          int colidx, bool inv, bool highlight)
	:
		attr(f.attr_bits() | (inv ? ATTR_INVERSE : 0)
		                   | (highlight ? ATTR_HIGHLIGHT : 0)),
		ascii(c),
		color(colidx & COLOR_MASK)
	{ }

	Font_face font_face() const
	{
		return Font_face((Font_face::Type)(attr & Font_face::attr_mask()));
	}

	int  colidx_fg() const { return color        & ATTR_COLIDX_MASK; }
	int  colidx_bg() const { return (color >> 3) & ATTR_COLIDX_MASK; }
	bool inverse()   const { return attr         & ATTR_INVERSE;     }
	bool highlight() const { return attr         & ATTR_HIGHLIGHT;   }

	void set_cursor()   { attr |=  ATTR_CURSOR; }
	void clear_cursor() { attr &= ~ATTR_CURSOR; }

	bool has_cursor() const { return attr & ATTR_CURSOR; }
};


class Char_cell_array_character_screen : public Terminal::Character_screen
{
	private:

		enum Cursor_visibility { CURSOR_INVISIBLE,
		                         CURSOR_VISIBLE,
		                         CURSOR_VERY_VISIBLE };

		Cell_array<Char_cell> &_char_cell_array;
		Terminal::Boundary     _boundary;
		Terminal::Position     _cursor_pos;
		/**
		 * Color index contains the fg color in the first 3 bits
		 * and the bg color in the second 3 bits (0bbbbfff).
		 */
		int                    _color_index;
		bool                   _inverse;
		bool                   _highlight;
		Cursor_visibility      _cursor_visibility;
		int                    _region_start;
		int                    _region_end;
		int                    _tab_size;

		enum { DEFAULT_COLOR_INDEX_BG = 0, DEFAULT_COLOR_INDEX = 7, DEFAULT_TAB_SIZE = 8 };

		struct Cursor_guard
		{
			Char_cell_array_character_screen &cs;

			Terminal::Position old_cursor_pos;

			Cursor_guard(Char_cell_array_character_screen &cs)
			:
				cs(cs), old_cursor_pos(cs._cursor_pos)
			{
				/* temporarily remove cursor */
				cs._char_cell_array.cursor(old_cursor_pos, false);
			}

			~Cursor_guard()
			{
				/* restore original cursor */
				cs._char_cell_array.cursor(old_cursor_pos, true);

				/* if cursor position changed, move cursor */
				Terminal::Position &new_cursor_pos = cs._cursor_pos;
				if (old_cursor_pos != new_cursor_pos) {

					cs._char_cell_array.cursor(old_cursor_pos, false, true);
					cs._char_cell_array.cursor(new_cursor_pos, true,  true);
				}
			}
		};

	public:

		Char_cell_array_character_screen(Cell_array<Char_cell> &char_cell_array)
		:
			_char_cell_array(char_cell_array),
			_boundary(_char_cell_array.num_cols(), _char_cell_array.num_lines()),
			_color_index(DEFAULT_COLOR_INDEX),
			_inverse(false),
			_highlight(false),
			_cursor_visibility(CURSOR_VISIBLE),
			_region_start(0),
			_region_end(_boundary.height - 1),
			_tab_size(DEFAULT_TAB_SIZE)
		{ }


		void _line_feed()
		{
			Cursor_guard guard(*this);

			_cursor_pos.y++;

			if (_cursor_pos.y > _region_end) {
				_char_cell_array.scroll_up(_region_start, _region_end);
				_cursor_pos.y = _region_end;
			}
		}

		void _carriage_return()
		{
			Cursor_guard guard(*this);

			_cursor_pos.x = 0;
		}


		/********************************
		 ** Character_screen interface **
		 ********************************/

		Terminal::Position cursor_pos() const { return _cursor_pos; }

		void output(Terminal::Character c)
		{
			if (c.ascii() > 0x10) {
				Cursor_guard guard(*this);
				_char_cell_array.set_cell(_cursor_pos.x, _cursor_pos.y,
				                          Char_cell(c.ascii(), Font_face::REGULAR,
				                          _color_index, _inverse, _highlight));
				_cursor_pos.x++;
			}

			switch (c.ascii()) {

			case '\r': /* 13 */
				_carriage_return();
				break;

			case '\n': /* 10 */
				_line_feed();
				_carriage_return();
				break;

			case 8: /* backspace */
				{
					Cursor_guard guard(*this);

					if (_cursor_pos.x > 0)
						_cursor_pos.x--;

					break;
				}

			case 9: /* tab */
				{
					Cursor_guard guard(*this);
					_cursor_pos.x += _tab_size - (_cursor_pos.x % _tab_size);
					break;
				}

			default:
				break;
			}

			if (_cursor_pos.x >= _boundary.width) {
				_carriage_return();
				_line_feed();
			}
		}

		void civis()
		{
			_cursor_visibility = CURSOR_INVISIBLE;
		}

		void cnorm()
		{
			_cursor_visibility = CURSOR_VISIBLE;
		}

		void cvvis()
		{
			_cursor_visibility = CURSOR_VERY_VISIBLE;
		}

		void cpr() { Genode::warning(__func__, " not implemented"); }

		void csr(int start, int end)
		{
			/* the arguments are specified use coordinate origin (1, 1) */
			start--;
			end--;

			_region_start = Genode::max(start, 0);
			_region_end   = Genode::min(end, _boundary.height - 1);

			/* preserve invariant of region size >= 0 */
			_region_end = Genode::max(_region_end, _region_start);
		}

		void cub(int dx)
		{
			Cursor_guard guard(*this);

			_cursor_pos.x -= dx;
			_cursor_pos.x = Genode::max(0, _cursor_pos.x);
		}

		void cuf(int dx)
		{
			Cursor_guard guard(*this);

			_cursor_pos.x += dx;
			_cursor_pos.x = Genode::min(_boundary.width - 1, _cursor_pos.x);
		}

		void cup(int y, int x)
		{
			Cursor_guard guard(*this);

			/* top-left cursor position is reported as (1, 1) */
			x--;
			y--;

			using namespace Genode;
			x = max(0, min(x, _boundary.width  - 1));
			y = max(0, min(y, _boundary.height - 1));

			_cursor_pos = Terminal::Position(x, y);
		}

		void cuu1()   { Genode::warning(__func__, " not implemented"); }
		void dch(int) { Genode::warning(__func__, " not implemented"); }

		void dl(int num_lines)
		{
			/* delete number of lines */
			for (int i = 0; i < num_lines; i++)
				_char_cell_array.scroll_up(_cursor_pos.y, _region_end);
		}

		void ech(int v)
		{
			Cursor_guard guard(*this);

			for (int i = 0; i < v; i++, _cursor_pos.x++)
				_char_cell_array.set_cell(_cursor_pos.x, _cursor_pos.y,
				                          Char_cell(' ', Font_face::REGULAR,
				                          _color_index, _inverse, _highlight));
		}

		void ed()
		{
			/* clear to end of screen */
			el();
			_char_cell_array.clear(_cursor_pos.y + 1, _boundary.height - 1);
		}

		void el()
		{
			/* clear to end of line */
			for (int x = _cursor_pos.x; x < _boundary.width; x++)
				_char_cell_array.set_cell(x, _cursor_pos.y, Char_cell());
		}

		void el1() { Genode::warning(__func__, " not implemented"); }

		void home()
		{
			Cursor_guard guard(*this);

			_cursor_pos = Terminal::Position(0, 0);
		}

		void hpa(int x)
		{
			Cursor_guard guard(*this);

			_cursor_pos.x = x;
			_cursor_pos.x = Genode::min(_boundary.width - 1, _cursor_pos.x);
		}

		void hts()    { Genode::warning(__func__, " not implemented"); }
		void ich(int) { Genode::warning(__func__, " not implemented"); }

		void il(int value)
		{
			Cursor_guard guard(*this);

			if (_cursor_pos.y > _region_end)
				return;

			_char_cell_array.cursor(_cursor_pos, false);

			for (int i = 0; i < value; i++)
				_char_cell_array.scroll_down(_cursor_pos.y, _region_end);

			_char_cell_array.cursor(_cursor_pos, true);
		}

		void oc() { Genode::warning(__func__, " not implemented"); }

		void op()
		{
			_color_index = DEFAULT_COLOR_INDEX | (DEFAULT_COLOR_INDEX_BG << 3);
		}

		void rc()   { Genode::warning(__func__, " not implemented"); }
		void ri()   { Genode::warning(__func__, " not implemented"); }
		void ris()  { Genode::warning(__func__, " not implemented"); }
		void rmam() { Genode::warning(__func__, " not implemented"); }
		void rmir() { Genode::warning(__func__, " not implemented"); }

		void setab(int value)
		{
			//_inverse = (value != 0);
			_color_index &= ~0x38; /* clear 111000 */
			_color_index |= (((value == 9) ? DEFAULT_COLOR_INDEX_BG : value) << 3);
		}

		void setaf(int value)
		{
			_color_index &= ~0x7; /* clear 000111 */
			_color_index |= (value == 9) ? DEFAULT_COLOR_INDEX : value;
		}

		void sgr(int value)
		{
			_highlight = (value & 0x1) != 0;
			_inverse   = (value & 0x2) != 0;

			/* sgr 0 is the command to reset all attributes, including color */
			if (value == 0)
				_color_index = DEFAULT_COLOR_INDEX | (DEFAULT_COLOR_INDEX_BG << 3);
		}

		void sgr0()
		{
			sgr(0);
		}

		void sc()         { Genode::warning(__func__, " not implemented"); }
		void smam()       { Genode::warning(__func__, " not implemented"); }
		void smir()       { Genode::warning(__func__, " not implemented"); }
		void tbc()        { Genode::warning(__func__, " not implemented"); }
		void u6(int, int) { Genode::warning(__func__, " not implemented"); }
		void u7()         { Genode::warning(__func__, " not implemented"); }
		void u8()         { Genode::warning(__func__, " not implemented"); }
		void u9()         { Genode::warning(__func__, " not implemented"); }

		void vpa(int y)
		{
			Cursor_guard guard(*this);

			_cursor_pos.y = y;
			_cursor_pos.y = Genode::min(_boundary.height - 1, _cursor_pos.y);
		}
};

#endif /* _TERMINAL__CHAR_CELL_ARRAY_CHARACTER_SCREEN_H_ */
