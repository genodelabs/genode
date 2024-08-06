/*
 * \brief   Scout tutorial browser main program
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/heap.h>

#include <scout/platform.h>
#include <scout/tick.h>
#include <scout/user_state.h>
#include <scout/graphics_backend_impl.h>

#include "config.h"
#include "elements.h"
#include "fade_icon.h"
#include "browser_window.h"

extern Scout::Document *create_document();

static Genode::Allocator *_alloc_ptr;

void *operator new (__SIZE_TYPE__ n) { return _alloc_ptr->alloc(n); }


#define POINTER_RGBA  _binary_pointer_rgba_start
#define NAV_NEXT_RGBA _binary_nav_next_rgba_start
#define NAV_PREV_RGBA _binary_nav_prev_rgba_start

extern unsigned char POINTER_RGBA[];
extern unsigned char NAV_NEXT_RGBA[];
extern unsigned char NAV_PREV_RGBA[];

static unsigned char *navicons_rgba[] = { NAV_NEXT_RGBA, NAV_PREV_RGBA };
static Scout::Generic_icon **navicons[] = { &Scout::Navbar::next_icon,
                                            &Scout::Navbar::prev_icon };

extern int native_startup(int, char **);


namespace Scout {
	struct Main;
	using namespace Genode;
}


struct Scout::Main : Scout::Event_handler
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	bool const _global_new_initialized = (_alloc_ptr = &_heap, true);

	bool const _launcher_initialized = (Launcher::init(_env, _heap), true);
	bool const _png_image_initialized = (Png_image::init(_heap), true);

	Gui::Connection _gui { _env };

	Platform _platform { _env, _gui.input };

	bool const _event_handler_registered = (_platform.event_handler(*this), true);

	Area  const _max_size         { 530, 620 };
	Point const _initial_position { 256, 80  };
	Area  const _initial_size     { 530, 400 };

	Config const _config { };

	Graphics_backend_impl
		_graphics_backend { _env.rm(), _gui, _heap, _max_size,
		                    _initial_position, _initial_size };

	void _init_navicons()
	{
		for (unsigned int i = 0; i < sizeof(navicons)/sizeof(void *); i++) {
			Fade_icon<Pixel_rgb888, 64, 64> *icon = new Fade_icon<Pixel_rgb888, 64, 64>;
			icon->rgba(navicons_rgba[i]);
			icon->alpha(100);
			*navicons[i] = icon;
		}
	}

	bool const _navicons_initialized = (_init_navicons(), true);

	Document &_doc = *create_document();

	/* create instance of browser window */
	Browser_window<Pixel_rgb888> _browser { &_doc, _graphics_backend,
	                                        _initial_position, _initial_size,
	                                        _max_size, _config };

	/* initialize mouse cursor */
	Icon<Pixel_rgb888, 32, 32> _mcursor { };

	void _init_mouse_cursor()
	{
		if (_config.mouse_cursor) {
			_mcursor.geometry(Rect(Point(0, 0), Area(32, 32)));
			_mcursor.rgba(POINTER_RGBA);
			_mcursor.alpha(255);
			_mcursor.findable(0);
			_browser.append(&_mcursor);
		}
	}

	bool const _mouse_cursor_initialized = (_init_mouse_cursor(), true);

	User_state _user_state { &_browser, &_browser,
	                         _initial_position.x, _initial_position.y };

	bool const _browser_ypos_initialized = (_browser.ypos(0), true);

	Scout::Point _mouse_position { };

	Genode::uint64_t _old_time = _platform.timer_ticks();

	void handle_event(Scout::Event const &event) override
	{
		using namespace Scout;

		Event ev = event;

		if (event.type != Event::WHEEL) {

			ev.mouse_position = ev.mouse_position - _user_state.view_position();

			/* update mouse cursor */
			if (_config.mouse_cursor && (ev.mouse_position.x != _mouse_position.x
			                          || ev.mouse_position.y != _mouse_position.y)) {
				int x1 = min(ev.mouse_position.x, _mouse_position.x);
				int y1 = min(ev.mouse_position.y, _mouse_position.y);
				int x2 = max(ev.mouse_position.x + _mcursor.size().w - 1,
				             _mouse_position.x + _mcursor.size().w - 1);
				int y2 = max(ev.mouse_position.y + _mcursor.size().h - 1,
				             _mouse_position.y + _mcursor.size().h - 1);

				_mcursor.geometry(Rect(ev.mouse_position, _mcursor.size()));
				_browser.redraw_area(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

				_mouse_position = ev.mouse_position;
			}
		}

		_user_state.handle_event(ev);

		if (event.type == Event::TIMER)
			Tick::handle((Scout::Tick::time)_platform.timer_ticks());

		/* perform periodic redraw */
		Genode::uint64_t curr_time = _platform.timer_ticks();
		if (!_platform.event_pending() && ((curr_time - _old_time > 20)
		                                || (curr_time < _old_time))) {
			_old_time = curr_time;
			_browser.process_redraw();
		}
	}

	Main(Env &env) : _env(env) { }
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Scout::Main main(env);
}
