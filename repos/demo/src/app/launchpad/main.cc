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


/**
 * Main program
 */
int main(int argc, char **argv)
{
	using namespace Scout;

	/* look for dynamic linker */
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		Genode::Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	static Nitpicker::Connection nitpicker;
	static Platform pf(*nitpicker.input());

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
		launchpad(
			graphics_backend, initial_position, initial_size, max_size,
			Genode::env()->ram_session()->avail()
		);

	/* request config file from ROM service */
	try {
		launchpad.process_config();
	} catch (...) { }

	Avail_quota_update avail_quota_update(&launchpad);

	/* create user state manager */
	static User_state user_state(&launchpad, &launchpad,
	                             initial_position.x(), initial_position.y());

	launchpad.parent(&user_state);
	launchpad.format(initial_size);
	launchpad.ypos(0);

	Genode::printf("--- entering main loop ---\n");

	/* enter main loop */
	unsigned long curr_time, old_time;
	curr_time = old_time = pf.timer_ticks();
	for (;;) {
		Event ev = pf.get_event();

		launchpad.gui_lock.lock();

		if (ev.type != Event::WHEEL)
			ev.mouse_position = ev.mouse_position - user_state.view_position();

		user_state.handle_event(ev);

		if (ev.type == Event::TIMER)
			Tick::handle(pf.timer_ticks());

		/* perform periodic redraw */
		curr_time = pf.timer_ticks();
		if (!pf.event_pending() && ((curr_time - old_time > 20) || (curr_time < old_time))) {
			old_time = curr_time;
			launchpad.process_redraw();
		}

		launchpad.gui_lock.unlock();

		if (ev.type == Event::QUIT)
			break;
	}

	return 0;
}
