/*
 * \brief  Test for decoding terminal input
 * \author Norman Feske
 * \date   2011-07-05
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/misc_math.h>
#include <util/string.h>

/* Terminal includes */
#include <terminal/decoder.h>


class Static_character_screen : public Terminal::Character_screen
{
	private:

		Terminal::Character_array &_char_array;
		Terminal::Boundary         _boundary;
		Terminal::Position         _cursor_pos;

	public:

		Static_character_screen(Terminal::Character_array &char_array)
		: _char_array(char_array), _boundary(_char_array.boundary()) { }

		void dump() const;


		/********************************
		 ** Character_screen interface **
		 ********************************/

		void output(Terminal::Character c)
		{
//			Genode::printf("output '%c'\n", c.ascii());

			if (c.ascii() > 0x10)
				_char_array.set(_cursor_pos, c);
			_cursor_pos.x++;
			if (_cursor_pos.x >= _boundary.width) {
				_cursor_pos.x = 0;
				_cursor_pos.y++;
			}
			if (_cursor_pos.y >= _boundary.height) {
//				_char_array.newline();
				_cursor_pos.y = _boundary.height - 1;
			}
		}

		void civis()
		{
		}

		void cnorm()
		{
		}

		void cvvis()
		{
		}

		void cpr()
		{
		}

		void csr(int, int)
		{
		}

		void cuf(int)
		{
			if (_cursor_pos.x >= _boundary.width)
				_cursor_pos.x++;
//
//			_cursor_pos = _cursor_pos + Terminal::Offset(1, 0);
		}

		void cup(int y, int x)
		{
			using namespace Genode;
			x = max(0, min(x, _boundary.width  - 1));
			y = max(0, min(y, _boundary.height - 1));
			_cursor_pos = Terminal::Position(x, y);
		}

		void cuu1()
		{
//			if (_cursor_pos.y > 0)
//				_cursor_pos.y--;
		}

		void dch(int)
		{
		}

		void dl(int)
		{
		}

		void ech(int)
		{
		}

		void ed()
		{
		}

		void el()
		{
		}

		void el1()
		{
		}

		void home()
		{
			_cursor_pos = Terminal::Position(0, 0);
		}

		void hpa(int x)
		{
			PDBG("hpa %d", x);
		}

		void hts()
		{
		}

		void ich(int)
		{
		}

		void il(int)
		{
		}

		void oc()
		{
		}

		void op()
		{
		}

		void rc()
		{
		}

		void ri()
		{
		}

		void ris()
		{
		}

		void rmam()
		{
		}

		void rmir()
		{
		}

		void setab(int)
		{
		}

		void setaf(int)
		{
		}

		void sgr(int)
		{
		}

		virtual void sgr0()
		{
		}

		void sc()
		{
		}

		void smam()
		{
		}

		void smir()
		{
		}

		void tbc()
		{
		}

		void u6(int, int)
		{
		}

		void u7()
		{
		}

		void u8()
		{
		}

		void u9()
		{
		}

		void vpa(int y)
		{
			PDBG("vpa %d", y);
		}
};


void Static_character_screen::dump() const
{
	using namespace Terminal;

	Genode::printf("--- screen dump follows ---\n");

	Boundary const boundary = _char_array.boundary();
	for (int y = 0; y < boundary.height; y++) {

		char line[boundary.width + 1];

		for (int x = 0; x < boundary.width; x++) {

			Character c = _char_array.get(Position(x, y));
			if (c.is_valid())
				line[x] = c.ascii();
			else
				line[x] = ' ';
		}

		line[boundary.width] = 0;
		Genode::printf("%s\n", line);
	}

	Genode::printf("--- end of screen dump ---\n");
}


extern "C" char _binary_vim_vt_start;
extern "C" char _binary_vim_vt_end;


int main(int argc, char **argv)
{
	using namespace Terminal;

	Static_character_array<81, 26> char_array;

	Static_character_screen screen(char_array);

	Decoder decoder(screen);
	for (char *c = &_binary_vim_vt_start; c < &_binary_vim_vt_end; c++) {
		decoder.insert(*c);
	}

	screen.dump();

	return 0;
}
