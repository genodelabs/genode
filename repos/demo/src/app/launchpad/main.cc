/*
 * \brief   Launchpad main program
 * \date    2006-08-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/component.h>

#include <scout/platform.h>
#include <scout/tick.h>
#include <scout/user_state.h>
#include <scout/printf.h>
#include <scout/nitpicker_graphics_backend.h>

#include "config.h"
#include "elements.h"
#include "launchpad_window.h"

#include <base/env.h>
#include <init/child_config.h>
#include <os/config.h>


/**
 * Runtime configuration
 */
namespace Scout { namespace Config {
	int iconbar_detail    = 1;
	int background_detail = 1;
	int mouse_cursor      = 1;
	int browser_attr      = 0;
} }


/**
 * Facility to keep the available quota display up-to-date
 */
class Avail_quota_update : public Scout::Tick
{
	private:

		Launchpad      *_launchpad;
		Genode::size_t  _avail;

	public:

		/**
		 * Constructor
		 */
		Avail_quota_update(Launchpad *launchpad):
			_launchpad(launchpad), _avail(0) {
			schedule(200); }

		/**
		 * Tick interface
		 */
		int on_tick()
		{
			Genode::size_t new_avail = Genode::env()->ram_session()->avail();

			/* update launchpad window if needed */
			if (new_avail != _avail)
				_launchpad->quota(new_avail);

			_avail = new_avail;

			/* schedule next tick */
			return 1;
		}
};


static long read_int_attr_from_config(const char *attr, long default_value)
{
	long result = default_value;
	try {
		Genode::config()->xml_node().attribute(attr).value(&result);
	} catch (...) { }
	return result;
}


struct Main : Scout::Event_handler
{
	Scout::Platform   &_pf;
	Scout::Window     &_launchpad;
	Scout::User_state &_user_state;

	unsigned long _old_time = _pf.timer_ticks();

	Main(Scout::Platform &pf, Scout::Window &launchpad, Scout::User_state &user_state)
	: _pf(pf), _launchpad(launchpad), _user_state(user_state) { }

	void handle_event(Scout::Event const &event) override
	{
		using namespace Scout;

		Event ev = event;

		if (ev.type != Event::WHEEL)
			ev.mouse_position = ev.mouse_position - _user_state.view_position();

		_user_state.handle_event(ev);

		if (ev.type == Event::TIMER)
			Tick::handle(_pf.timer_ticks());

		/* perform periodic redraw */
		unsigned long const curr_time = _pf.timer_ticks();
		if (!_pf.event_pending() && ((curr_time - _old_time > 20)
		                          || (curr_time < _old_time))) {
			_old_time = curr_time;
			_launchpad.process_redraw();
		}
	}
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env)
{
	using namespace Scout;

	static Nitpicker::Connection nitpicker(env);
	static Platform pf(env, *nitpicker.input());

	long initial_x = read_int_attr_from_config("xpos",   550);
	long initial_y = read_int_attr_from_config("ypos",   150);
	long initial_w = read_int_attr_from_config("width",  400);
	long initial_h = read_int_attr_from_config("height", 400);

	Area  const max_size        (530, 620);
	Point const initial_position(initial_x, initial_y);
	Area  const initial_size    (initial_w, initial_h);

	static Nitpicker_graphics_backend
		graphics_backend(nitpicker, max_size, initial_position, initial_size);

	/* create instance of launchpad window */
	static Launchpad_window<Pixel_rgb565>
		launchpad(env, graphics_backend, initial_position, initial_size,
		          max_size, env.ram().avail());

	/* request config file from ROM service */
	try { launchpad.process_config(); } catch (...) { }

	static Avail_quota_update avail_quota_update(&launchpad);

	/* create user state manager */
	static User_state user_state(&launchpad, &launchpad,
	                             initial_position.x(), initial_position.y());

	launchpad.parent(&user_state);
	launchpad.format(initial_size);
	launchpad.ypos(0);

	static Main main(pf, launchpad, user_state);
	pf.event_handler(main);
}
