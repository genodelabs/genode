/*
 * \brief  C++ wrapper for ncurses API
 * \author Norman Feske
 * \date   2013-02-21
 */

#ifndef _NCURSES_CXX_H_
#define _NCURSES_CXX_H_

namespace Genode { class Allocator; }

class Ncurses
{
	private:

		Genode::Allocator &_alloc;

		unsigned _columns;
		unsigned _lines;

	public:

		class Window
		{
			private:

				struct Ncurses_window;

				friend class Ncurses;

				Ncurses_window * const _window;

				int _w;

				Window(unsigned x, unsigned y, unsigned w, unsigned h);

			public:

				~Window();

				void move_cursor(unsigned x, unsigned y);

				void print_char(unsigned long const c, bool highlight, bool inverse);

				void refresh();

				void erase();

				void horizontal_line(int line);
		};

		Window *create_window(int x, int y, int w, int h);
		void destroy_window(Ncurses::Window *window);

		void clear_ok();

		void do_update();

		Ncurses(Genode::Allocator &);

		void cursor_visible(bool);

		int read_character();

		unsigned columns() const { return _columns; }
		unsigned lines()   const { return _lines; }
};

#endif /* _NCURSES_CXX_H_ */
