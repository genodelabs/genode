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

/**
 * Local includes
 */
#include "config.h"
#include "elements.h"
#include "platform.h"
#include "canvas_rgb565.h"
#include "fade_icon.h"
#include "tick.h"
#include "redraw_manager.h"
#include "user_state.h"
#include "browser_window.h"

extern Document *create_document();


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


#define POINTER_RGBA  _binary_pointer_rgba_start
#define NAV_NEXT_RGBA _binary_nav_next_rgba_start
#define NAV_PREV_RGBA _binary_nav_prev_rgba_start

extern unsigned char POINTER_RGBA[];
extern unsigned char NAV_NEXT_RGBA[];
extern unsigned char NAV_PREV_RGBA[];

static unsigned char *navicons_rgba[] = { NAV_NEXT_RGBA, NAV_PREV_RGBA };
static Generic_icon **navicons[] = { &Navbar::next_icon, &Navbar::prev_icon };

extern int native_startup(int, char **);


/**
 * Main program
 */
int main(int argc, char **argv)
{
	if (native_startup(argc, argv)) return -1;

	/* init platform */
	static Platform pf(256, 80, 530, 620);

	/* initialize icons for navigation bar */
	for (unsigned int i = 0; i < sizeof(navicons)/sizeof(void *); i++) {
		Fade_icon<Pixel_rgb565, 64, 64> *icon = new Fade_icon<Pixel_rgb565, 64, 64>;
		icon->rgba(navicons_rgba[i]);
		icon->alpha(100);
		*navicons[i] = icon;
	}

	static Document *doc = create_document();

	/* init canvas */
	static Chunky_canvas<Pixel_rgb565> canvas;
	canvas.init(static_cast<Pixel_rgb565 *>(pf.buf_adr()),
	            pf.scr_w()*pf.scr_h());
	canvas.set_size(pf.scr_w(), pf.scr_h());
	canvas.clip(0, 0, pf.scr_w(), pf.scr_h());

	/* init redraw manager */
	static Redraw_manager redraw(&canvas, &pf, pf.vw(), pf.vh(), true);

	/* create instance of browser window */
	static Browser_window<Pixel_rgb565> browser
	(
		doc,                     /* initial document       */
		&pf,                     /* platform               */
		&redraw,                 /* redraw manager object  */
		pf.scr_w(), pf.scr_h(),  /* max size of window     */
		Config::browser_attr
	);

	/* initialize mouse cursor */
	int mx = 0, my = 0;
	static Icon<Pixel_rgb565, 32, 32> mcursor;
	if (Config::mouse_cursor) {
		mcursor.geometry(mx, my, 32, 32);
		mcursor.rgba(POINTER_RGBA);
		mcursor.alpha(255);
		mcursor.findable(0);
		browser.append(&mcursor);
	}

	/* create user state manager */
	static User_state user_state(&browser, &browser, pf.vx(), pf.vy());

	/* assign browser as root element to redraw manager */
	redraw.root(&browser);

	browser.ypos(0);

	/* enter main loop */
	Event ev;
	unsigned long curr_time, old_time;
	curr_time = old_time = pf.timer_ticks();
	do {
		pf.get_event(&ev);

		if (ev.type != Event::WHEEL) {
			ev.mx -= user_state.vx();
			ev.my -= user_state.vy();

			/* update mouse cursor */
			if (Config::mouse_cursor && (ev.mx != mx || ev.my != my)) {
				int x1 = min(ev.mx, mx);
				int y1 = min(ev.my, my);
				int x2 = max(ev.mx + mcursor.w() - 1, mx + mcursor.w() - 1);
				int y2 = max(ev.my + mcursor.h() - 1, my + mcursor.h() - 1);

				mcursor.geometry(ev.mx, ev.my, mcursor.w(), mcursor.h());
				redraw.request(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

				mx = ev.mx; my = ev.my;
			}
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

	} while (ev.type != Event::QUIT);

	return 0;
}
