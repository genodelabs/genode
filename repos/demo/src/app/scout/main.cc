/*
 * \brief   Scout tutorial browser main program
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/component.h>

#include <scout/platform.h>
#include <scout/tick.h>
#include <scout/user_state.h>
#include <scout/nitpicker_graphics_backend.h>

#include "config.h"
#include "elements.h"
#include "fade_icon.h"
#include "browser_window.h"

extern Scout::Document *create_document();


/**
 * Runtime configuration
 */
namespace Scout { namespace Config {
	int iconbar_detail    = 1;
	int background_detail = 1;
	int mouse_cursor      = 1;
	int browser_attr      = 0;
} }


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


namespace Scout { struct Main; }


struct Scout::Main : Scout::Event_handler
{
	Scout::Platform   &_pf;
	Scout::Window     &_browser;
	Scout::User_state &_user_state;
	Scout::Element    &_mcursor;

	Scout::Point _mouse_position;

	unsigned long _old_time = _pf.timer_ticks();

	Main(Scout::Platform &pf, Scout::Window &browser,
	     Scout::User_state &user_state, Scout::Element &mcursor)
	:
		_pf(pf), _browser(browser), _user_state(user_state), _mcursor(mcursor)
	{  }

	void handle_event(Scout::Event const &event) override
	{
		using namespace Scout;

		Event ev = event;

		if (event.type != Event::WHEEL) {

			ev.mouse_position = ev.mouse_position - _user_state.view_position();

			/* update mouse cursor */
			if (Config::mouse_cursor && (ev.mouse_position.x() != _mouse_position.x()
			                          || ev.mouse_position.y() != _mouse_position.y())) {
				int x1 = min(ev.mouse_position.x(), _mouse_position.x());
				int y1 = min(ev.mouse_position.y(), _mouse_position.y());
				int x2 = max(ev.mouse_position.x() + _mcursor.size().w() - 1,
				             _mouse_position.x() + _mcursor.size().w() - 1);
				int y2 = max(ev.mouse_position.y() + _mcursor.size().h() - 1,
				             _mouse_position.y() + _mcursor.size().h() - 1);

				_mcursor.geometry(Rect(ev.mouse_position, _mcursor.size()));
				_browser.redraw_area(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

				_mouse_position = ev.mouse_position;
			}
		}

		_user_state.handle_event(ev);

		if (event.type == Event::TIMER)
			Tick::handle(_pf.timer_ticks());

		/* perform periodic redraw */
		unsigned long curr_time = _pf.timer_ticks();
		if (!_pf.event_pending() && ((curr_time - _old_time > 20)
		                          || (curr_time < _old_time))) {
			_old_time = curr_time;
			_browser.process_redraw();
		}
	}
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env)
{
	using namespace Scout;

	Launcher::init(env);

	static Nitpicker::Connection nitpicker;
	static Platform pf(env, *nitpicker.input());

	Area  const max_size(530, 620);
	Point const initial_position(256, 80);
	Area  const initial_size(530, 400);

	Config::mouse_cursor = 0;
	Config::browser_attr = 7;

	static Nitpicker_graphics_backend
		graphics_backend(nitpicker, max_size, initial_position, initial_size);

	/* initialize icons for navigation bar */
	for (unsigned int i = 0; i < sizeof(navicons)/sizeof(void *); i++) {
		Fade_icon<Pixel_rgb565, 64, 64> *icon = new Fade_icon<Pixel_rgb565, 64, 64>;
		icon->rgba(navicons_rgba[i]);
		icon->alpha(100);
		*navicons[i] = icon;
	}

	static Document *doc = create_document();

	/* create instance of browser window */
	static Browser_window<Pixel_rgb565> browser
	(
		doc,
		graphics_backend,
		initial_position,
		initial_size,
		max_size,
		Config::browser_attr
	);

	/* initialize mouse cursor */
	static Icon<Pixel_rgb565, 32, 32> mcursor;
	if (Config::mouse_cursor) {
		mcursor.geometry(Rect(Point(0, 0), Area(32, 32)));
		mcursor.rgba(POINTER_RGBA);
		mcursor.alpha(255);
		mcursor.findable(0);
		browser.append(&mcursor);
	}

	/* create user state manager */
	static User_state user_state(&browser, &browser,
	                             initial_position.x(), initial_position.y());
	browser.ypos(0);

	static Main main(pf, browser, user_state, mcursor);
	pf.event_handler(main);

}
