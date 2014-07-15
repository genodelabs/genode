/*
 * \brief  Input handling utilities
 * \author Norman Feske
 * \date   2013-09-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INPUT_H_
#define _INPUT_H_

/* Genode includes */
#include <input/event.h>

/* local includes */
#include "user_state.h"


/**
 * Determine number of events that can be merged into one
 *
 * \param ev   pointer to first event array element to check
 * \param max  size of the event array
 * \return     number of events subjected to merge
 */
static unsigned num_consecutive_events(Input::Event const *ev, unsigned max)
{
	if (max < 1) return 0;
	if (ev->type() != Input::Event::MOTION) return 1;

	bool first_is_absolute = ev->is_absolute_motion();

	/* iterate until we get a different event type, start at second */
	unsigned cnt = 1;
	for (ev++ ; cnt < max; cnt++, ev++) {
		if (ev->type() != Input::Event::MOTION) break;
		if (first_is_absolute != ev->is_absolute_motion()) break;
	}
	return cnt;
}


/**
 * Merge consecutive motion events
 *
 * \param ev  event array to merge
 * \param n   number of events to merge
 * \return    merged motion event
 */
static Input::Event merge_motion_events(Input::Event const *ev, unsigned n)
{
	Input::Event res;
	for (unsigned i = 0; i < n; i++, ev++)
		res = Input::Event(Input::Event::MOTION, 0, ev->ax() == -1 ? res.ax() : ev->ax(), ev->ay() == -1 ? res.ay() : ev-> ay(),
		                   res.rx() + ev->rx(), res.ry() + ev->ry());
	return res;
}


static void import_input_events(Input::Event *ev_buf, unsigned num_ev,
                                User_state &user_state, Canvas_base &canvas)
{
	/*
	 * Take events from input event buffer, merge consecutive motion
	 * events, and pass result to the user state.
	 */
	for (unsigned src_ev_cnt = 0; src_ev_cnt < num_ev; src_ev_cnt++) {

		Input::Event *e = &ev_buf[src_ev_cnt];
		Input::Event curr = *e;

		if (e->type() == Input::Event::MOTION) {
			unsigned n = num_consecutive_events(e, num_ev - src_ev_cnt);
			curr = merge_motion_events(e, n);

			/* skip merged events */
			src_ev_cnt += n - 1;
		}

		/*
		 * If subsequential relative motion events are merged to
		 * a zero-motion event, drop it. Otherwise, it would be
		 * misinterpreted as absolute event pointing to (0, 0).
		 */
		if (e->is_relative_motion() && curr.rx() == 0 && curr.ry() == 0)
			continue;

		/* pass event to user state */
		user_state.handle_event(curr, canvas);
	}
}

#endif /* _INPUT_H_ */
