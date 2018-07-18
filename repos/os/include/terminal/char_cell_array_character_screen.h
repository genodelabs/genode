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

	enum { ATTR_COLIDX_MASK = 0x07U,
	       ATTR_CURSOR      = 0x10U,
	       ATTR_INVERSE     = 0x20U,
	       ATTR_HIGHLIGHT   = 0x40U };

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

	unsigned colidx_fg() const { return color        & ATTR_COLIDX_MASK; }
	unsigned colidx_bg() const { return (color >> 3) & ATTR_COLIDX_MASK; }
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

		enum Irm { REPLACE, INSERT };

		Cell_array<Char_cell> &_char_cell_array;
		Terminal::Boundary     _boundary;
		Terminal::Position     _cursor_store { };
		Terminal::Position     _cursor_pos   { };

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

		Irm                    _irm = REPLACE;

		bool                   _wrap = false;

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
					cs._char_cell_array.cursor(
						new_cursor_pos, cs._cursor_visibility != CURSOR_INVISIBLE,  true);
				}
			}
		};

		static void _missing(char const *method_name)
		{
			Genode::warning(method_name, " not implemented");
		}

		static void _missing(char const *method_name, int arg)
		{
			Genode::warning(method_name, " not implemented for ", arg);
		}

		void _new_line()
		{
			Cursor_guard guard(*this);

			_cursor_pos.x = 0;
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


		/********************************
		 ** Character_screen interface **
		 ********************************/

		Terminal::Position cursor_pos() const { return _cursor_pos; }

		void cursor_pos(Terminal::Position pos)
		{
			_cursor_pos.x = Genode::min(_boundary.width  - 1, pos.x);
			_cursor_pos.y = Genode::min(_boundary.height - 1, pos.y);
		}

		void output(Terminal::Character c)
		{
			if (_irm == INSERT)
				_missing("insert mode");

			switch (c.ascii()) {

			case '\n': /* 10 */
				_new_line();
				break;

			case '\r': /* 13 */
				_carriage_return();
				break;

			/* 14: shift-out */
			/* 15: shift-in  */

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
				if (0x1f < c.ascii() && c.ascii() < 0x7f) {
					Cursor_guard guard(*this);
					_char_cell_array.set_cell(_cursor_pos.x, _cursor_pos.y,
					                          Char_cell(c.ascii(), Font_face::REGULAR,
					                          _color_index, _inverse, _highlight));
					_cursor_pos.x++;
				}
				break;
			}

			if (_cursor_pos.x >= _boundary.width) {
				if (_wrap) {
					_new_line();
				} else {
					_cursor_pos.x = _boundary.width-1;
				}
			}
		}

		void cha(int pn) override
		{
			Cursor_guard guard(*this);
			_cursor_pos.x = Genode::max(pn - 1, _boundary.width);
		}

		void civis() override
		{
			_cursor_visibility = CURSOR_INVISIBLE;
			_char_cell_array.cursor(_cursor_pos, false);
		}

		void cnorm() override
		{
			_cursor_visibility = CURSOR_VISIBLE;
			_char_cell_array.cursor(_cursor_pos, true);
		}

		void cvvis() override
		{
			_cursor_visibility = CURSOR_VERY_VISIBLE;
			_char_cell_array.cursor(_cursor_pos, true);
		}

		void csr(int start, int end) override
		{
			/* the arguments are specified use coordinate origin (1, 1) */
			start--;
			end--;

			_region_start = Genode::max(start, 0);
			_region_end   = Genode::min(end, _boundary.height - 1);

			/* preserve invariant of region size >= 0 */
			_region_end = Genode::max(_region_end, _region_start);
		}

		void cub(int dx) override
		{
			Cursor_guard guard(*this);

			_cursor_pos.x -= dx;
			_cursor_pos.x = Genode::max(0, _cursor_pos.x);
		}

		void cud(int dy) override
		{
			Cursor_guard guard(*this);

			_cursor_pos.y += dy;
			_cursor_pos.y = Genode::min(_boundary.height - 1, _cursor_pos.y);
		}

		void cuf(int dx) override
		{
			Cursor_guard guard(*this);

			_cursor_pos.x += dx;
			_cursor_pos.x = Genode::min(_boundary.width - 1, _cursor_pos.x);
		}

		void cup(int y, int x) override
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

		void cuu(int dy) override
		{
			Cursor_guard guard(*this);

			_cursor_pos.x += dy;
			_cursor_pos.y = Genode::max(0, _cursor_pos.y);
		}

		void da(int) override { _missing(__func__); }

		void dch(int pn) override
		{
			pn = Genode::min(_boundary.width - _cursor_pos.x, pn);
			for (int x = _cursor_pos.x; x < _boundary.width; ++x) {
				_char_cell_array.set_cell(x, _cursor_pos.y,
					_char_cell_array.get_cell(x+pn, _cursor_pos.y));
			}
			for (int x = _boundary.width - pn; x < _boundary.width; ++x) {
				_char_cell_array.set_cell(x, _cursor_pos.y, Char_cell());
			}
		}

		void dl(int num_lines) override
		{
			/* delete number of lines */
			for (int i = 0; i < num_lines; i++)
				_char_cell_array.scroll_up(_cursor_pos.y, _region_end);
		}

		/**
		 * Erase character
		 */
		void ech(int pn)
		{
			int y = _cursor_pos.y;
			int x = _cursor_pos.x;

			do {
				while (x < _boundary.width && pn) {
					_char_cell_array.set_cell(x++, y, Char_cell());
					--pn;
				}
				x = 0;
				++y;
			} while (pn);
		}

		/**
		 * Erase in page
		 */
		void ed(int ps) override
		{
			/* clear to end of screen */
			switch(ps) {

			case 0:
				for (int x = _cursor_pos.x; x < _boundary.width; ++x)
					_char_cell_array.set_cell(x, _cursor_pos.y, Char_cell());
				_char_cell_array.clear(_cursor_pos.y + 1, _boundary.height-1);
				return;

			case 1:
				_char_cell_array.clear(0, _cursor_pos.y-1);
				for (int x = 0; x <= _cursor_pos.x; ++x)
					_char_cell_array.set_cell(x, _cursor_pos.y, Char_cell());
				return;

			case 2:
				_char_cell_array.clear(0, _boundary.height-1);
				return;

			default:
				Genode::warning(__func__, " not implemented for ", ps);
				break;
			}
		}

		void el(int ps) override
		{
			switch (ps) {
			case 0: /* clear to end of line */
				for (int x = _cursor_pos.x; x < _boundary.width; ++x)
					_char_cell_array.set_cell(x, _cursor_pos.y, Char_cell());
				return;

			case 1: /* clear from begining of line */
				for (int x = 0; x <= _cursor_pos.x; ++x)
					_char_cell_array.set_cell(x, _cursor_pos.y, Char_cell());
				return;

			case 2:
				_char_cell_array.clear(_cursor_pos.y, _cursor_pos.y);
				return;

			default: _missing(__func__, ps);
			}
		}

		void enacs() override { _missing(__func__); }
		void flash() override { _missing(__func__); }

		void home() override
		{
			Cursor_guard guard(*this);

			_cursor_pos.x = 0;
		}

		void hts() override
		{
			_tab_size = _cursor_pos.x;
		}

		void ich(int pn) override
		{
			pn = Genode::min(_boundary.width - _cursor_pos.x, pn);

			for (int x = _boundary.width-1; _cursor_pos.x+pn < x; --x) {
				_char_cell_array.set_cell(x, _cursor_pos.y,
					_char_cell_array.get_cell(x-1, _cursor_pos.y));
			}
			for (int i = 0; i < pn; ++i) {
				_char_cell_array.set_cell(
					_cursor_pos.x+i, _cursor_pos.y, Char_cell());
			}
		}

		void il(int value) override
		{
			Cursor_guard guard(*this);

			if (_cursor_pos.y > _region_end)
				return;

			_char_cell_array.cursor(_cursor_pos, false);

			for (int i = 0; i < value; i++)
				_char_cell_array.scroll_down(_cursor_pos.y, _region_end);

			_char_cell_array.cursor(_cursor_pos, true);
		}

		void is2() override { _missing(__func__); }
		void nel() override { _missing(__func__); }

		void op() override
		{
			_color_index = DEFAULT_COLOR_INDEX | (DEFAULT_COLOR_INDEX_BG << 3);
		}

		void rm(int ps) override
		{
			switch (ps) {
			case 4: /* INSERTION REPLACEMENT MODE */
				_irm = REPLACE;
				break;
			case 34: /* cursor visibility */
				return cnorm();
			default:
				_missing(__func__, ps);
				break;
			}
		}

		void sm(int ps) override
		{
			switch (ps) {
			case 4: /* INSERTION REPLACEMENT MODE */
				_irm = INSERT;
				break;
			case 34: /* cursor visibility */
				return civis();
				break;
			default:
				_missing(__func__, ps);
				break;
			}
		}

		void rc()    override { _missing(__func__); }
		void rs2()   override { _missing(__func__); }
		void rmir()  override { _missing(__func__); }
		void rmcup() override { }
		void rmkx()  override { }

		void sd(int pn) override
		{
			for (int i = 0; i < pn; ++i)
			_char_cell_array.scroll_down(_region_start, _region_end);
		}

		void setab(int value) override
		{
			_color_index &= ~0x38; /* clear 111000 */
			_color_index |= (((value == 9) ? DEFAULT_COLOR_INDEX_BG : value) << 3);
		}

		void setaf(int value) override
		{
			_color_index &= ~0x7; /* clear 000111 */
			_color_index |= (value == 9) ? DEFAULT_COLOR_INDEX : value;
		}

		void sgr(int value) override
		{
			switch (value) {
			case 0:
				/* sgr 0 is the command to reset all attributes, including color */
				_highlight = false;
				_inverse = false;
				_color_index = DEFAULT_COLOR_INDEX | (DEFAULT_COLOR_INDEX_BG << 3);
				break;
			case 1:
				_highlight = true;
				break;
			case 7:
				_inverse = true;
				break;
			default:
				break;
			}
		}

		void smcup() override { }
		void smir()  override { _missing(__func__); }
		void smkx()  override { }

		void su(int pn) override
		{
			for (int i = 0; i < pn; ++i)
				_char_cell_array.scroll_up(_region_start, _region_end);
		}

		void tbc()    override { _missing(__func__); }

		void tsr(int pn) override
		{
			_missing(__func__, pn);
			/*
			int x = pn;
			for (int y = _cursor_pos.y; y < _boundary.height-1; ++y) {
				for (int i = 0; i < _tab_size; ++i) {

				}
			}
			 */
		}

		void vpa(int pn)
		{
			Cursor_guard guard(*this);
			_cursor_pos.x = pn;
		}

		void vpb(int pn)
		{
			Cursor_guard guard(*this);
			_cursor_pos.x = Genode::min(0, _cursor_pos.x - pn);
		}

		void decsc() override
		{
			_cursor_store = _cursor_pos;
		}

		void decrc() override
		{
			Cursor_guard guard(*this);
			_cursor_pos = _cursor_store;
		}

		void decsm(int p1, int) override
		{
			switch (p1) {
			case    1: _missing("Application Cursor Keys"); return; //return smkx();
			case    7: _wrap = false; return;
			case   25:
			case   34: return cnorm(); // Visible Cursor
			case 1000: return _missing("VT200 mouse tracking");
			case 1002: return _missing("xterm butten event mouse");
			case 1003: return _missing("xterm any event mouse");
			case 1049: return _missing("Alternate Screen (new xterm code)"); //smcup();
			default: break;
			}
			_missing(__func__, p1);
		}

		void decrm(int p1, int) override
		{
			switch (p1) {
			case    1: _missing("Application Cursor Keys"); return; //return rmkx();
			case    7: _wrap = true; return;
			case   25:
			case   34: return civis();
			case 1000: return _missing("VT200 mouse tracking"); //rs2();
			case 1002: return _missing("xterm butten event mouse");
			case 1003: return _missing("xterm any event mouse");
			case 1049: return _missing("Alternate Screen (new xterm code)"); //rmcup();
			default: break;
			}
			_missing(__func__, p1);
		}

		void scs_g0(int charset) override { _missing(__func__, charset); }

		void scs_g1(int charset) override { _missing(__func__, charset); }

		void reverse_index()
		{
			Cursor_guard guard(*this);
			if (_cursor_pos.y) {
				_cursor_pos.y = _cursor_pos.y - 1;
			} else {
				_char_cell_array.scroll_down(_region_start, _region_end);
			}
		};
};

#endif /* _TERMINAL__CHAR_CELL_ARRAY_CHARACTER_SCREEN_H_ */
