/*
 * \brief  XEvent tracker
 * \author Norman Feske
 * \date   2009-11-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <X11/Xlib.h>

enum { MAX_VIEWS = 100 };

extern int config_force_top;


/******************************************************
 ** Functions provided by the X window event tracker **
 ******************************************************/

/**
 * Initialize window-event tracking facility
 *
 * \return true on success
 */
bool xev_track_init(Display *dpy);

/**
 * Evaluate X event for window or screen updates
 */
void xev_track_handle_event(Display *dpy, XEvent &ev);

/**
 * Track dirty pixels caused by the mouse cursor
 */
void xev_track_handle_cursor(Display *dpy);


/******************************************************
 ** Functions called from the X window event tracker **
 ******************************************************/

/**
 * Called when a window is created
 */
extern void create_view(int view_id);

/**
 * Called when a window get destroyed
 */
extern void destroy_view(int view_id);

/**
 * Called to define the view that displays the desktop
 */
extern void set_background_view(int view_id);

/**
 * Called when a window gets a new size or position
 */
extern void place_view(int view_id, int x, int y, int w, int h);

/**
 * Move view to another view-stacking position
 */
extern void stack_view(int view_id, int neighbor_id, bool behind);

/**
 * Refresh screen region
 */
extern void refresh(int x, int y, int w, int h);
