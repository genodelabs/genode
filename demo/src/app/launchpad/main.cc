/*
 * \brief   Launchpad main program
 * \date    2006-08-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "config.h"
#include "elements.h"
#include "platform.h"
#include "canvas_rgb565.h"
#include "tick.h"
#include "redraw_manager.h"
#include "user_state.h"
#include "launchpad_window.h"
#include "printf.h"

#include <base/env.h>
#include <init/child_config.h>
#include <os/config.h>


/**
 * Runtime configuration
 */
namespace Config
{
	int iconbar_detail    = 1;
	int background_detail = 1;
	int mouse_cursor      = 1;
	int browser_attr      = 0;
}

extern int native_startup(int, char **);


/**
 * Facility to keep the available quota display up-to-date
 */
class Avail_quota_update : public Tick
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


/**
 * Process launchpad XML configuration
 */
static void process_config(Launchpad *launchpad)
{
	using namespace Genode;

	Xml_node config_node = config()->xml_node();

	/*
	 * Iterate through all entries of the config file and create
	 * launchpad entries as specified.
	 */
	int launcher_cnt = 0;
	for (unsigned i = 0; i < config_node.num_sub_nodes(); i++) {
		Xml_node node = config_node.sub_node(i);
		if (node.has_type("launcher"))

			/* catch XML syntax errors within launcher node */
			try {
				/* read file name and default quote from launcher node */
				Xml_node filename_node = node.sub_node("filename");

				size_t filename_len = filename_node.content_size();
				char *filename = (char *)env()->heap()->alloc(filename_len + 1);
				if (!filename) {
					::printf("Error: Out of memory while processing configuration\n");
					return;
				}
				filename_node.value(filename, filename_len + 1);
				Xml_node ram_quota_node = node.sub_node("ram_quota");
				Number_of_bytes default_ram_quota = 0;
				ram_quota_node.value(&default_ram_quota);

				/* obtain configuration for the child */
				Init::Child_config *config = new (env()->heap())
					Init::Child_config(Genode::env()->ram_session_cap(), node);

				/* add launchpad entry */
				launchpad->add_launcher(filename, default_ram_quota,
				                        config->dataspace());
				launcher_cnt++;

			} catch (...) {
				::printf("Warning: Launcher entry %d is malformed.\n",
				         launcher_cnt + 1);
			}
		else {
			char buf[32];
			node.type_name(buf, sizeof(buf));
			::printf("Warning: Ignoring unsupported tag <%s>.\n", buf);
		}
	}
}


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
	using namespace Genode;

	if (native_startup(argc, argv)) return -1;

	/* look for dynamic linker */
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		Genode::Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	long initial_x = read_int_attr_from_config("xpos",   550);
	long initial_y = read_int_attr_from_config("ypos",   150);
	long initial_w = read_int_attr_from_config("width",  400);
	long initial_h = read_int_attr_from_config("height", 400);

	/* init platform */
	static Platform pf(initial_x, initial_y, initial_w, initial_h, 400);

	/* init canvas */
	static Chunky_canvas<Pixel_rgb565> canvas;
	canvas.init(static_cast<Pixel_rgb565 *>(pf.buf_adr()),
	            pf.scr_w()*pf.scr_h());
	canvas.set_size(pf.scr_w(), pf.scr_h());
	canvas.clip(0, 0, pf.scr_w(), pf.scr_h());

	/* init redraw manager */
	static Redraw_manager redraw(&canvas, &pf, pf.vw(), pf.vh());

	/* create instance of launchpad window */
	static Launchpad_window<Pixel_rgb565>
		launchpad(
			&pf, &redraw, pf.scr_w(), pf.scr_h(),
			env()->ram_session()->avail()
		);

	/* request config file from ROM service */
	try {
		process_config(&launchpad);

	/* if there exists no configuration, use defaults */
	} catch (...) {
		launchpad.add_launcher("testnit",      768*1024);
		launchpad.add_launcher("scout",    11*1024*1024);
		launchpad.add_launcher("launchpad", 6*1024*1024);
		launchpad.add_launcher("nitlog",    1*1024*1024);
		launchpad.add_launcher("liquid_fb", 7*1024*1024);
		launchpad.add_launcher("nitpicker", 1*1024*1024);
	}

	Avail_quota_update avail_quota_update(&launchpad);

	/* create user state manager */
	static User_state user_state(&launchpad, &launchpad, pf.vx(), pf.vy());

	/* assign launchpad window as root element to redraw manager */
	redraw.root(&launchpad);

	pf.view_geometry(pf.vx(), pf.vy(), pf.vw(), pf.vh());
	launchpad.parent(&user_state);
	launchpad.format(pf.vw(), pf.vh());
	launchpad.ypos(0);

	Genode::printf("--- entering main loop ---\n");

	/* enter main loop */
	Event ev;
	unsigned long curr_time, old_time;
	curr_time = old_time = pf.timer_ticks();
	do {
		pf.get_event(&ev);

		launchpad.gui_lock.lock();

		if (ev.type != Event::WHEEL) {
			ev.mx -= user_state.vx();
			ev.my -= user_state.vy();
		}

		user_state.handle_event(ev);

		if (ev.type == Event::REFRESH)
			pf.scr_update(0, 0, pf.scr_w(), pf.scr_h());

		if (ev.type == Event::TIMER)
			Tick::handle(pf.timer_ticks());

		/* perform periodic redraw */
		curr_time = pf.timer_ticks();
		if (!pf.event_pending() && ((curr_time - old_time > 20) || (curr_time < old_time))) {
			old_time = curr_time;
			redraw.process();
		}

		launchpad.gui_lock.unlock();

	} while (ev.type != Event::QUIT);

	return 0;
}
