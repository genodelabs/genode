/*
 * \brief  X event tracker, sending X11 events via ioctl to OKlinux's screen driver
 * \author Stefan Kalkowski
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Standard includes */
#include <stdio.h>
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

/* X11 includes */
#include <X11/XWDFile.h>
#include <X11/Xlib.h>

/* X event-tracker includes */
#include <xev_track/xev_track.h>

/* OKlinux kernel includes */
#include <oklx/ioctl.h>

/* App-specific includes */
#include "bounding_box.h"

int config_force_top =  1;          /* evaluated by the X event-tracker library */
int nitpicker_fd     = -1;          /* nitpicker screen device file-descriptor */
static Bounding_box pending_redraw; /* collects refresh operations */


/**
 * C++ wrapper around C struct 'genode_screen_region'
 */
struct Genode_screen_region : public genode_screen_region
{
	Genode_screen_region() { }
	Genode_screen_region(int ix, int iy, int iw, int ih) {
		x = ix, y = iy, w = iw, h = ih; }

	bool operator != (Genode_screen_region &r) {
		return x != r.x || y != r.y || w != r.w || h != r.h; }
};


struct View_state
{
	Genode_screen_region flushed;  /* last flushed view position */
	Genode_screen_region curr;     /* current view position */

	bool to_be_flushed() { return flushed != curr; }
};


static View_state view_states[MAX_VIEWS];


static bool view_id_is_valid(int view_id) {
	return view_id > 0 && view_id < MAX_VIEWS; }


/*******************************
 ** X event-tracker callbacks **
 *******************************/

void create_view(int view_id) {
	if(ioctl(nitpicker_fd, NITPICKER_IOCTL_CREATE_VIEW, view_id))
		printf("An error occured! errno=%d\n", errno);
}


void destroy_view(int view_id) {
	if(ioctl(nitpicker_fd, NITPICKER_IOCTL_DESTROY_VIEW, view_id))
		printf("An error occured! errno=%d\n", errno);
}


void set_background_view(int view_id) {
	if(ioctl(nitpicker_fd, NITPICKER_IOCTL_BACK_VIEW, view_id))
		printf("An error occured! errno=%d\n", errno);
}


void place_view(int view_id, int x, int y, int w, int h) {
	if (view_id_is_valid(view_id))
		view_states[view_id].curr = Genode_screen_region(x, y, w, h);
}


void stack_view(int view_id, int neighbor_id, bool behind) {
	struct genode_view_stack st = { view_id, neighbor_id, behind };
	if(ioctl(nitpicker_fd, NITPICKER_IOCTL_STACK_VIEW, &st))
		printf("An error occured! errno=%d\n", errno);
}


void refresh(int x, int y, int w, int h)
{
	pending_redraw.extend(x, y, w, h);
}


static void flush(int x, int y, int width, int height)
{
	if (width <= 0 || height <= 0) return;

	struct genode_screen_region reg = { x, y, width, height };
	if(ioctl(nitpicker_fd, FRAMEBUFFER_IOCTL_REFRESH, &reg))
		printf("An error occured! errno=%d\n", errno);

	for (int i = 0; i < MAX_VIEWS; i++) {
		if (!view_states[i].to_be_flushed()) continue;

		struct genode_view_place place = { i, view_states[i].curr };
		if(ioctl(nitpicker_fd, NITPICKER_IOCTL_PLACE_VIEW, &place))
			printf("An error occured! errno=%d\n", errno);

		view_states[i].flushed = view_states[i].curr;
	}
}


static void flush_view_placements()
{
	for (int i = 0; i < MAX_VIEWS; i++) {
		if (!view_states[i].to_be_flushed()) continue;

		struct genode_view_place place = { i, view_states[i].curr };
		if(ioctl(nitpicker_fd, NITPICKER_IOCTL_PLACE_VIEW, &place))
			printf("An error occured! errno=%d\n", errno);

		view_states[i].flushed = view_states[i].curr;
	}
}


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

	/* open nitpicker screen device */
	if((nitpicker_fd = open("/dev/fb1", O_WRONLY)) < 0)
		return -7;

	/* init event tracker library */
	if (!xev_track_init(dpy))
		return -6;

	/* busy loop polls X events */
	for (;;) {
		pending_redraw.reset();

		XEvent ev;
		while (XPending(dpy)) {
			XNextEvent(dpy, &ev);
			xev_track_handle_event(dpy, ev);
		}

		flush_view_placements();
		xev_track_handle_cursor(dpy);

		if (pending_redraw.valid())
			flush(pending_redraw.x(), pending_redraw.y(),
			      pending_redraw.w(), pending_redraw.h());

		usleep(10000);
	}
	return 0;
}
