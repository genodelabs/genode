/*
 * \brief  Window-event tracker for the X Window System
 * \author Norman Feske
 * \date   2009-11-04
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <signal.h>
#include <stdio.h>

/* X11 includes */
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>

/* xev includes */
#include <xev_track/xev_track.h>


struct View
{
	bool tracked;  /* flag indicating that the view is in use */
	Window xwin;
	Window above;
	int x, y, w, h, border;

	/**
	 * Default constructor
	 *
	 * Make sure that the window is initially marked as free
	 */
	View() : tracked(false) { }
};


static View   views[MAX_VIEWS];
static Window root;
static int    xdamage_ev;
static Damage damage;


/***************
 ** Utilities **
 ***************/

/**
 * Allocate view ID
 *
 * \return allocated view ID, or
 *         -1 if allocation failed
 */
static int alloc_view_id()
{
	int i;
	for (i = 0; i < MAX_VIEWS; i++)
		if (!views[i].tracked) break;

	if (i == MAX_VIEWS)
		return -1;

	views[i].tracked = true;
	return i;
}


/**
 * Mark view ID as free
 */
static void release_view_id(int view_id)
{
	views[view_id].tracked = false;
}


/**
 * Find view ID for a given X window ID
 */
static int find_view_id(Window xwin)
{
	/* search for window with matchin xwin id */
	for (int i = 0; i < MAX_VIEWS; i++)
		if (views[i].xwin == xwin && views[i].tracked)
			return i;

	return -1;
}


/**
 * Create new window
 *
 * \param position  -1 .. at top,
 *                  -2 .. background,
 *                  otherwise window id of the neighbor behind the new
 *                  window
 */
static void assign_window(int view_id, Window xwin, Display *dpy, int position)
{
	/* sanity check */
	if (view_id < 0) return;

	View *view = &views[view_id];

	view->xwin = xwin;

	/* request position and size of new window */
	XWindowAttributes attr;
	XGetWindowAttributes(dpy, xwin, &attr);
	view->x      = attr.x;
	view->y      = attr.y;
	view->w      = attr.width;
	view->h      = attr.height;
	view->border = attr.border_width;

	create_view(view_id);

	if (position == -2) {
		stack_view(view_id, -1, false);
		set_background_view(view_id);
	}

	if (position == -1)
		stack_view(view_id, -1, true);

	if (position >= 0)
		stack_view(view_id, position, true);

	place_view(view_id, view->x, view->y, view->w + view->border*2,
	                                      view->h + view->border*2);
}


/**
 * Scan all currently present windows
 */
static void scan_windows(Display *dpy, Window root)
{
	Window dummy_w1, dummy_w2, *win_list;
	unsigned int num_wins;
	XQueryTree(dpy, root, &dummy_w1, &dummy_w2, &win_list, &num_wins);

	for (unsigned i = 0; i < num_wins; i++) {

		XWindowAttributes attr;
		XGetWindowAttributes(dpy, win_list[i], &attr);

		if (attr.map_state != IsViewable)
			continue;

		int view_id = alloc_view_id();
		if (view_id >= 0)
			assign_window(view_id, win_list[i], dpy, -1);
	}
	XFree(win_list);

	/* listen at the root window */
	XSelectInput(dpy, root, SubstructureNotifyMask | PointerMotionMask);
}


/**
 * Find view belonging to the window in front of the specified X window
 *
 * \return view ID, or
 *         -1 if no matching view exists
 */
static int find_view_in_front(Display *dpy, Window root, Window win)
{
	Window dummy_w1, dummy_w2, *win_list;
	unsigned int num_wins, i;

	XQueryTree(dpy, root, &dummy_w1, &dummy_w2, &win_list, &num_wins);

	/* find window in X window stack */
	for (i = 0; i < num_wins; i++)
		if (win_list[i] == win)
			break;

	/* skip current window */
	i++;

	/* find and return view belonging to the X window */
	int curr;
	for (; i < num_wins; i++)
		if ((curr = find_view_id(win_list[i])))
			return curr;

	return -1;
}


static void get_pointer_pos(Display *dpy, int *mx, int *my)
{
	Window   dummy_win;
	int      dummy_int;
	unsigned dummy_uint;
	XQueryPointer(dpy, root, &dummy_win, &dummy_win,
	              mx, my, &dummy_int, &dummy_int, &dummy_uint);
}


struct Mouse_cursor_tracker
{
	private:

		enum { CURSOR_WIDTH = 20, CURSOR_HEIGHT = 20 };
		int _x1, _y1, _x2, _y2;
		bool _valid;

	public:

	Mouse_cursor_tracker() : _valid(false) { }

	void reset(int x, int y)
	{
		_x1 = x - CURSOR_WIDTH;
		_y1 = y - CURSOR_HEIGHT;
		_x2 = x + CURSOR_WIDTH;
		_y2 = y + CURSOR_HEIGHT;
		_valid = false;
	}

	void track(int x, int y)
	{
		if (_x1 > x - CURSOR_WIDTH)  _x1 = x - CURSOR_WIDTH;
		if (_y1 > y - CURSOR_HEIGHT) _y1 = y - CURSOR_HEIGHT;
		if (_x2 < x + CURSOR_WIDTH)  _x2 = x + CURSOR_WIDTH;
		if (_y2 < y + CURSOR_HEIGHT) _y2 = y + CURSOR_HEIGHT;
		_valid = true;
	}

	bool bounding_box(int *x, int *y, int *w, int *h)
	{
		*x = _x1;
		*y = _y1;
		*w = _x2 - _x1 + 1;
		*h = _y2 - _y1 + 1;
		return _valid;
	}
};

struct Mouse_cursor_tracker mouse_cursor_tracker;


/****************************
 ** Top-window enforcement **
 ****************************/

/*
 * Some window managers do not raise a window that is already on top. This is
 * bad because there may be overlay windows that are not known to the X window
 * system but that cover the topmost X window. Thus, we want always to receive
 * a top event. For this, we create a dedicated invisible window that stays
 * always on top of all others. The topmost real X window is always the second.
 * Therefore, the window manager thinks that this window can be topped and
 * generates the desired event.
 */
static Window topwin;


static bool window_left_of_screen(Display *dpy, Window xwin)
{
	XWindowAttributes attr;
	XGetWindowAttributes(dpy, xwin, &attr);

	return (attr.x + attr.width <= 0);
}


enum {
	MAGIC_WIN_X = 2000, MAGIC_WIN_Y = 2000,
	MAGIC_WIN_W =  1, MAGIC_WIN_H =  1
};


/**
 * Create magic window that stays always on top
 */
static void create_magic_topwin(Display *dpy, Window root)
{
	XWindowChanges wincfg;

	/* create magic window that stays always on top */
	topwin = XCreateWindow(dpy, root,
	                       MAGIC_WIN_X, MAGIC_WIN_Y,  /* position */
	                       MAGIC_WIN_W, MAGIC_WIN_H,  /* size     */
	                       0,                         /* border   */
	                       CopyFromParent,            /* depth    */
	                       InputOutput,               /* class    */
	                       CopyFromParent,            /* visual   */
	                       0, 0);

	wincfg.x = MAGIC_WIN_X;
	wincfg.y = MAGIC_WIN_Y;
	XConfigureWindow(dpy, topwin,  CWX | CWY , &wincfg);
	XMapWindow(dpy, topwin);
	wincfg.x = MAGIC_WIN_X;
	wincfg.y = MAGIC_WIN_Y;
	XConfigureWindow(dpy, topwin,  CWX | CWY , &wincfg);


	int view_id = alloc_view_id();
	if (view_id >= 0)
		assign_window(view_id, topwin, dpy, root);
}


/**
 * Bring magic window in front of all others
 */
static void raise_magic_window(Display *dpy)
{
	XRaiseWindow(dpy, topwin);

	/*
	 * Some window managers tend to relocate existing windows on startup.  So
	 * let's re-position the window to make sure that it remains invisible in
	 * such cases.
	 */
	XMoveWindow(dpy, topwin, -200, -200);
}


/**********************
 ** X event handling **
 **********************/

static void handle_xwindow_event(Display *dpy, Window root, XEvent &ev)
{
	int view_id, behind_id;
	int x, y, w, h;
	View *view;

	switch (ev.type) {

	case MotionNotify:

		x = ev.xmotion.x_root;
		y = ev.xmotion.y_root;
		mouse_cursor_tracker.track(x, y);
		break;

	case ConfigureNotify:

		if ((view_id = find_view_id(ev.xconfigure.window)) < 0)
			break;

		x = ev.xconfigure.x;
		y = ev.xconfigure.y;
		w = ev.xconfigure.width;
		h = ev.xconfigure.height;

		view = &views[view_id];

		/*
		 * If window position and size keeps the same,
		 * we assume, the window has been topped.
		 */
		if (x == view->x && y == view->y && w == view->w && h == view->h) {

			int behind_id = find_view_in_front(dpy, root, ev.xconfigure.window);

			stack_view(view_id, behind_id, true);

			if (!window_left_of_screen(dpy, ev.xconfigure.window) && config_force_top)
				raise_magic_window(dpy);

		} else {

			/* keep track new window position */
			view->x = x; view->y = y; view->w = w; view->h = h;

			place_view(view_id, x, y, w + view->border*2, h + view->border*2);
		}
		break;

	case Expose:

		if ((view_id = find_view_id(ev.xconfigure.window)) < 0)
			break;

		stack_view(view_id, -1, true);
		break;

	case UnmapNotify:

		if ((view_id = find_view_id(ev.xconfigure.window)) < 0)
			break;

		/* avoid destroying a view twice */
		if (!views[view_id].tracked)
			break;

		destroy_view(view_id);
		release_view_id(view_id);
		break;

	case MapNotify:

		if ((view_id = find_view_id(ev.xconfigure.window)) >= 0) {
			printf("MapRequest: window already present - view ID %d\n", view_id);
			break;
		}

		behind_id = find_view_in_front(dpy, root, ev.xconfigure.window);

		/*
		 * Idea: Call XQueryTree to obtain the position where the new
		 *       window is located in the window stack.
		 *
		 */

		view_id = alloc_view_id();
		if (view_id >= 0)
			assign_window(view_id, ev.xconfigure.window, dpy, behind_id);

		if (!window_left_of_screen(dpy, ev.xconfigure.window) && config_force_top)
			raise_magic_window(dpy);
		break;
	}
}


static void handle_xdamage_event(Display *dpy, XEvent &ev)
{
	if (ev.type != XDamageNotify + xdamage_ev)
		return;

	static XserverRegion region = XFixesCreateRegion (dpy, 0, 0);
	static XserverRegion part   = XFixesCreateRegion (dpy, 0, 0);

	XDamageNotifyEvent *dev = (XDamageNotifyEvent *)&ev;

	XFixesSetRegion(dpy, part, &dev->area, 1);
	XFixesUnionRegion(dpy, region, region, part);
	XFlush(dpy);

	XRectangle *rects;
	int         nrects;

	nrects = 0;
	rects  = XFixesFetchRegion (dpy, region, &nrects);

	for (int i = 0; i < nrects; i++)
		refresh(rects[i].x, rects[i].y, rects[i].width, rects[i].height);

	/* clear collected damage region from damage */
	XDamageSubtract (dpy, damage, region, None);

	/* empty collected region */
	XFixesSetRegion (dpy, region, 0, 0);
}


/**
 * Error handler that is called on xlib errors
 */
static int x_error_handler(Display *dpy, XErrorEvent *r)
{
	printf("Error: x_error_handler called\n");
	return 0;
}


/**********************
 ** Public interface **
 **********************/

void xev_track_handle_event(Display *dpy, XEvent &ev)
{
	handle_xwindow_event(dpy, root, ev);
	handle_xdamage_event(dpy, ev);
}


void xev_track_handle_cursor(Display *dpy)
{
	int x = 0, y = 0, w = 0, h = 0;
	static int old_mx, old_my;
	int new_mx, new_my;

	get_pointer_pos(dpy, &new_mx, &new_my);
	if (new_mx != old_mx || new_my != old_my)
		mouse_cursor_tracker.track(new_mx, new_my);

	if (mouse_cursor_tracker.bounding_box(&x, &y, &w, &h))
		refresh(x, y, w, h);

	mouse_cursor_tracker.reset(old_mx, old_my);
	mouse_cursor_tracker.track(new_mx, new_my);

	old_mx = new_mx, old_my = new_my;
}


bool xev_track_init(Display *dpy)
{
	XSetErrorHandler(x_error_handler);

	int screen = DefaultScreen(dpy);
	root       = RootWindow(dpy, screen);

	int error;
	if (!XDamageQueryExtension (dpy, &xdamage_ev, &error)) {
		printf("Error: Could not query Xdamage extension\n");
		return false;
	}

	damage = XDamageCreate(dpy, root, XDamageReportBoundingBox);

	if (config_force_top)
		create_magic_topwin(dpy, root);

	/* create background view */
	int bg_view_id = alloc_view_id();
	if (bg_view_id >= 0)
		assign_window(bg_view_id, root, dpy, -2);

	/* retrieve information about currently present windows */
	scan_windows(dpy, root);

	return true;
}
