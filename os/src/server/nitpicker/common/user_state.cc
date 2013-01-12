/*
 * \brief  User state implementation
 * \author Norman Feske
 * \date   2006-08-27
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <input/event.h>
#include <input/keycodes.h>
#include "user_state.h"

using namespace Input;

/*
 * Definition of magic keys
 */
enum { KILL_KEY = KEY_PRINT      };
enum { XRAY_KEY = KEY_SCROLLLOCK };


/***************
 ** Utilities **
 ***************/

static inline bool _masked_key(int keycode) {
	return keycode == KILL_KEY || keycode == XRAY_KEY; }


static inline bool _mouse_button(int keycode) {
		return keycode >= BTN_LEFT && keycode <= BTN_MIDDLE; }


/**************************
 ** User state interface **
 **************************/

User_state::User_state(Canvas *canvas, Menubar *menubar):
	View_stack(canvas, this), _key_cnt(0), _menubar(menubar),
	_pointed_view(0) { }


void User_state::handle_event(Input::Event ev)
{
	/*
	 * Mangle incoming events
	 */

	int keycode = ev.code();
	int ax = _mouse_pos.x(), ay = _mouse_pos.y();
	int rx = 0, ry = 0; /* skip info about relative motion per default */

	/* KEY_PRINT and KEY_SYSRQ both enter kill mode */
	if ((ev.type() == Event::PRESS) && (ev.code() == KEY_SYSRQ))
		keycode = KEY_PRINT;

	/* transparently handle absolute and relative motion events */
	if (ev.type() == Event::MOTION) {
		if ((ev.rx() || ev.ry()) && ev.ax() == 0 && ev.ay() == 0) {
			ax = max(0, min(size().w(), ax + ev.rx()));
			ay = max(0, min(size().h(), ay + ev.ry()));
		} else {
			ax = ev.ax();
			ay = ev.ay();
		}
	}

	/* propagate relative motion for wheel events */
	if (ev.type() == Event::WHEEL) {
		rx = ev.rx();
		ry = ev.ry();
	}

	/* create the mangled event */
	ev = Input::Event(ev.type(), keycode, ax, ay, rx, ry);

	_mouse_pos = Point(ax, ay);

	View *pointed_view = find_view(_mouse_pos);

	/*
	 * Deliver a leave event if pointed-to session changed
	 */
	if (pointed_view && _pointed_view &&
		pointed_view->session() != _pointed_view->session()) {
		Input::Event leave_ev(Input::Event::LEAVE, 0, ax, ay, 0, 0);
		_pointed_view->session()->submit_input_event(&leave_ev);
	}

	/* remember currently pointed-at view */
	_pointed_view = pointed_view;

	/*
	 * We expect pointed view to be always defined. In the worst case (with no
	 * view at all), the pointed view is the background.
	 */

	bool update_all = false;

	/*
	 * Detect mouse press event in kill mode, used to select the session
	 * to lock out.
	 */
	if (kill() && ev.type() == Event::PRESS && ev.code() == Input::BTN_LEFT) {
		if (pointed_view && pointed_view->session())
			lock_out_session(pointed_view->session());

		/* leave kill mode */
		pointed_view = 0;
		Mode::_mode &= ~KILL;
		update_all = true;
	}

	if (ev.type() == Event::PRESS && _key_cnt == 0) {

		/* update focused view */
		if (pointed_view != focused_view()
		 && _mouse_button(ev.code())) {

			bool const focus_stays_in_session =
				(_focused_view && pointed_view &&
				 _focused_view->session() == pointed_view->session());

			/*
			 * Update the whole screen when the focus change results in
			 * changing the focus to another session.
			 */
			if (flat() && !focus_stays_in_session)
				update_all = true;

			/*
			 * Notify both the old focussed session and the new one.
			 */
			if (!focus_stays_in_session) {

				if (_focused_view) {
					Input::Event unfocus_ev(Input::Event::FOCUS, 0, ax, ay, 0, 0);
					_focused_view->session()->submit_input_event(&unfocus_ev);
				}

				if (pointed_view) {
					Input::Event focus_ev(Input::Event::FOCUS, 1, ax, ay, 0, 0);
					pointed_view->session()->submit_input_event(&focus_ev);
				}
			}

			if (!flat() || !_focused_view || !pointed_view)
				update_all = true;

			_focused_view = pointed_view;
		}

		/* toggle kill and xray modes */
		if (ev.code() == KILL_KEY || ev.code() == XRAY_KEY) {

			Mode::_mode ^= ev.code() == KILL_KEY ? KILL : XRAY;
			update_all = true;
		}
	}

	if (update_all) {

		if (focused_view() && focused_view()->session())
			_menubar->state(*this, focused_view()->session()->label(),
			                focused_view()->title(),
			                focused_view()->session()->color());
		else
			_menubar->state(*this, "", "", BLACK);

		update_all_views();
	}

	/* count keys */
	if (ev.type() == Event::PRESS) _key_cnt++;
	if (ev.type() == Event::RELEASE && _key_cnt > 0) _key_cnt--;

	/*
	 * Deliver event to Nitpicker session.
	 * (except when kill mode is activated)
	 */

	if (kill()) return;

	if (ev.type() == Event::MOTION || ev.type() == Event::WHEEL) {

		if (_key_cnt == 0) {

			/*
			 * In flat mode, we deliver motion events to the session of
			 * the pointed view. In xray mode, we deliver motion
			 * events only to the session with the focused view.
			 */
			if (flat() || (xray() && focused_view() == pointed_view))
				if (pointed_view)
					pointed_view->session()->submit_input_event(&ev);

		} else if (focused_view())
			focused_view()->session()->submit_input_event(&ev);
	}

	/* deliver press/release event to session with focused view */
	if (ev.type() == Event::PRESS || ev.type() == Event::RELEASE)
		if (!_masked_key(ev.code()) && focused_view())
			focused_view()->session()->submit_input_event(&ev);
}


/********************
 ** Mode interface **
 ********************/

void User_state::forget(View *v)
{
	if (_focused_view == v) {
		Mode::forget(v);
		_menubar->state(*this, "", "", BLACK);
		update_all_views();
	}
	if (_pointed_view == v)
		_pointed_view = find_view(_mouse_pos);
}

