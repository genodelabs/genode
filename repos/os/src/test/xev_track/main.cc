/*
 * \brief  Test for X event tracker, dumping X11 events
 * \author Norman Feske
 * \date   2010-02-11
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* X11 includes */
#include <X11/XWDFile.h>
#include <X11/Xlib.h>

/* X event-tracker includes */
#include <xev_track/xev_track.h>

/* standard includes */
#include <stdio.h>


/*****************************
 ** configuration variables **
 *****************************/

int config_force_top = 1;  /* evaluated by the X event-tracker library */ 

static int config_dump_refresh = false;



/*******************************
 ** X event-tracker callbacks **
 *******************************/

void create_view(int view_id) {
	printf("create_view(view_id=%d)\n", view_id); }


void destroy_view(int view_id) {
	printf("destroy_view(view_id=%d)\n", view_id); }


void set_background_view(int view_id) {
	printf("set_background_view(view_id=%d)\n", view_id); }


void place_view(int view_id, int x, int y, int w, int h) {
	printf("place_view(view_id=%d, x=%d, y=%d, w=%d, h=%d)\n",
	       view_id, x, y, w, h); }


void stack_view(int view_id, int neighbor_id, bool behind) {
	printf("stack_view(view_id=%d, neighbor_id=%d, behind=%d)\n",
	       view_id, neighbor_id, behind); }


void refresh(int x, int y, int w, int h) {
	if (config_dump_refresh)
		printf("refresh(x=%d, y=%d, w=%d, h=%d)\n", x, y, w, h); }


/**
 * Main program
 */
int main()
{
	/* create connection to the X server */
	Display *dpy = XOpenDisplay(":0");
	if (!dpy) {
		printf("Error: Cannot open display\n");
		return -4;
	}

	/* init event tracker library */
	if (!xev_track_init(dpy))
		return -6;

	/* busy loop polls X events */
	for (;;) {
		XEvent ev;
		XNextEvent(dpy, &ev);

		/*
		 * By calling this function, the callbacks defined above are
		 * triggered.
		 */
		xev_track_handle_event(dpy, ev);
		xev_track_handle_cursor(dpy);
	}

	return 0;
}

