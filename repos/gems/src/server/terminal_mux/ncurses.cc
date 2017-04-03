/*
 * \brief  C++ wrapper for ncurses API
 * \author Norman Feske
 * \date   2013-02-21
 */

/* Genode includes */
#include <base/log.h>
#include <base/allocator.h>

/* libc and ncurses includes */
#include <ncurses.h>
#include <stdlib.h>    /* for 'setenv()' */
#include <sys/types.h> /* for 'open()' */
#include <fcntl.h>
#include <unistd.h>    /* for 'dup2()' */

/* local includes */
#include <ncurses_cxx.h>


struct Ncurses::Window::Ncurses_window : WINDOW { };


Ncurses::Window::Window(unsigned x, unsigned y, unsigned w, unsigned h)
:
	_window(static_cast<Ncurses::Window::Ncurses_window *>(newwin(h, w, y, x))),
	_w(w)
{ }


Ncurses::Window::~Window()
{
	delwin(_window);
}


void Ncurses::Window::move_cursor(unsigned x, unsigned y)
{
	wmove(_window, y, x);
}


void Ncurses::Window::print_char(unsigned long const c, bool highlight, bool inverse)
{
	waddch(_window, c | (highlight ? A_STANDOUT : 0)
	                  | (inverse   ? A_REVERSE  : 0));
}


void Ncurses::Window::refresh()
{
	wnoutrefresh(_window);
}


void Ncurses::Window::erase()
{
	werase(_window);
}


void Ncurses::Window::horizontal_line(int line)
{
	mvwhline(_window, line, 0, ' ' | A_REVERSE, _w);
}


Ncurses::Window *Ncurses::create_window(int x, int y, int w, int h)
{
	return new (_alloc) Ncurses::Window(x, y, w, h);
}


void Ncurses::destroy_window(Ncurses::Window *window)
{
	Genode::destroy(_alloc, window);
}


void Ncurses::clear_ok()
{
	clearok(stdscr, true);
}


void Ncurses::do_update()
{
	doupdate();
}


void Ncurses::cursor_visible(bool visible)
{
	if (!visible)
		wmove(stdscr, _lines - 1, 0);
}


int Ncurses::read_character()
{
	return getch();
}


Ncurses::Ncurses(Genode::Allocator &alloc) : _alloc(alloc)
{
	/*
	 * Redirect stdio to terminal
	 */
	char const *device_name = "/dev/terminal";
	int fd = open(device_name, O_RDWR);
	if (fd < 0) {
		Genode::error("could not open ", device_name);
		return;
	}
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);

	setenv("TERM", "vt102", 1);

	initscr();
	nonl();
	noecho();
	nodelay(stdscr, true);
	cbreak();
	getmaxyx(stdscr, _lines, _columns);
}

