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


/***************
 ** Utilities **
 ***************/

static inline bool _masked_key(Global_keys &global_keys, Keycode keycode) {
	return global_keys.is_kill_key(keycode) || global_keys.is_xray_key(keycode); }


static inline bool _mouse_button(Keycode keycode) {
		return keycode >= BTN_LEFT && keycode <= BTN_MIDDLE; }


/**************************
 ** User state interface **
 **************************/

User_state::User_state(Global_keys &global_keys, Canvas &canvas, Menubar &menubar)
:
	View_stack(canvas, *this), _global_keys(global_keys), _key_cnt(0),
	_menubar(menubar), _pointed_view(0), _input_receiver(0),
	_global_key_sequence(false)
{ }


void User_state::handle_event(Input::Event ev)
{
	Input::Keycode     const keycode = ev.keycode();
	Input::Event::Type const type    = ev.type();

	/*
	 * Mangle incoming events
	 */
	int ax = _mouse_pos.x(), ay = _mouse_pos.y();
	int rx = 0, ry = 0; /* skip info about relative motion by default */

	/* transparently handle absolute and relative motion events */
	if (type == Event::MOTION) {
		if ((ev.rx() || ev.ry()) && ev.ax() == 0 && ev.ay() == 0) {
			ax = max(0, min(size().w(), ax + ev.rx()));
			ay = max(0, min(size().h(), ay + ev.ry()));
		} else {
			ax = ev.ax();
			ay = ev.ay();
		}
	}

	/* propagate relative motion for wheel events */
	if (type == Event::WHEEL) {
		rx = ev.rx();
		ry = ev.ry();
	}

	/* create the mangled event */
	ev = Input::Event(type, keycode, ax, ay, rx, ry);

	_mouse_pos = Point(ax, ay);

	/* count keys */
	if (type == Event::PRESS)                   _key_cnt++;
	if (type == Event::RELEASE && _key_cnt > 0) _key_cnt--;

	View const * const pointed_view = find_view(_mouse_pos);

	/*
	 * Deliver a leave event if pointed-to session changed
	 */
	if (pointed_view && _pointed_view &&
		!pointed_view->same_session_as(*_pointed_view)) {

		Input::Event leave_ev(Input::Event::LEAVE, 0, ax, ay, 0, 0);
		_pointed_view->session().submit_input_event(leave_ev);
	}

	_pointed_view = pointed_view;

	/**
	 * Guard that, when 'enabled' is set to true, performs a whole-screen
	 * update when leaving the scope
	 */
	struct Update_all_guard
	{
		User_state &user_state;
		bool        enabled;
		char const *menu_title;

		Update_all_guard(User_state &user_state)
		: user_state(user_state), enabled(false), menu_title("") { }

		~Update_all_guard()
		{
			if (!enabled)
				return;

			if (user_state._input_receiver)
				user_state._menubar.state(user_state,
				                          user_state._input_receiver->label().string(),
				                          menu_title,
				                          user_state._input_receiver->color());
			else
				user_state._menubar.state(user_state, "", "", BLACK);

			user_state.update_all_views();
		}
	} update_all_guard(*this);

	/*
	 * Handle start of a key sequence
	 */
	if (type == Event::PRESS && _key_cnt == 1) {

		/*
		 * Detect mouse press event in kill mode, used to select the session
		 * to lock out.
		 */
		if (kill() && keycode == Input::BTN_LEFT) {
			if (pointed_view)
				lock_out_session(pointed_view->session());

			/* leave kill mode */
			update_all_guard.enabled = true;
			Mode::leave_kill();
			return;
		}

		/* update focused view */
		if (pointed_view != focused_view() && _mouse_button(keycode)) {

			bool const focus_stays_in_session =
				focused_view() && pointed_view &&
				focused_view()->belongs_to(pointed_view->session());

			/*
			 * Update the whole screen when the focus change results in
			 * changing the focus to another session.
			 */
			if (flat() && !focus_stays_in_session)
				update_all_guard.enabled = true;

			/*
			 * Notify both the old focussed session and the new one.
			 */
			if (!focus_stays_in_session) {

				if (focused_view()) {
					Input::Event unfocus_ev(Input::Event::FOCUS, 0, ax, ay, 0, 0);
					focused_view()->session().submit_input_event(unfocus_ev);
				}

				if (pointed_view) {
					Input::Event focus_ev(Input::Event::FOCUS, 1, ax, ay, 0, 0);
					pointed_view->session().submit_input_event(focus_ev);
				}
			}

			if (!flat() || !focused_view() || !pointed_view)
				update_all_guard.enabled = true;

			focused_view(pointed_view);
		}

		/*
		 * If there exists a global rule for the pressed key, set the
		 * corresponding session as receiver of the input stream until the key
		 * count reaches zero. Otherwise, the input stream is directed to the
		 * pointed-at view.
		 *
		 * If we deliver a global key sequence, we temporarily change the focus
		 * to the global receiver. To reflect that change, we need to update
		 * the whole screen.
		 */
		Session * const global_receiver = _global_keys.global_receiver(keycode);
		if (global_receiver) {
			_global_key_sequence        = true;
			_input_receiver             = global_receiver;
			update_all_guard.menu_title = "";
			update_all_guard.enabled    = true;
		}

		/*
		 * No global rule matched, so the input stream gets directed to the
		 * focused view or refers to a built-in operation.
		 */
		if (!global_receiver && focused_view()) {
			_input_receiver             = &focused_view()->session();
			update_all_guard.menu_title =  focused_view()->title();
		}

		/*
		 * Toggle kill and xray modes. If one of those keys is pressed,
		 * suppress the delivery to clients.
		 */
		if (_global_keys.is_operation_key(keycode)) {

			if (_global_keys.is_kill_key(keycode)) Mode::toggle_kill();
			if (_global_keys.is_xray_key(keycode)) Mode::toggle_xray();

			update_all_guard.enabled = true;
			_input_receiver          = 0;
		}
	}

	/*
	 * Deliver event to session except when kill mode is activated
	 */
	if (kill()) return;

	if (type == Event::MOTION || type == Event::WHEEL) {

		if (_key_cnt == 0) {

			/*
			 * In flat mode, we deliver motion events to the session of
			 * the pointed view. In xray mode, we deliver motion
			 * events only to the session with the focused view.
			 */
			if (flat() || (xray() && focused_view() == pointed_view))
				if (pointed_view)
					pointed_view->session().submit_input_event(ev);

		} else if (_input_receiver)
			_input_receiver->submit_input_event(ev);
	}

	/* deliver press/release event to session with focused view */
	if (type == Event::PRESS || type == Event::RELEASE)
		if (_input_receiver)
			_input_receiver->submit_input_event(ev);

	/*
	 * Detect end of global key sequence
	 */
	if (ev.type() == Event::RELEASE && _key_cnt == 0 && _global_key_sequence) {
		_input_receiver             = focused_view() ? &focused_view()->session() : 0;
		update_all_guard.menu_title = focused_view() ?  focused_view()->title()   : "";
		update_all_guard.enabled    = true;
		_global_key_sequence        = false;
	}
}


/********************
 ** Mode interface **
 ********************/

void User_state::forget(View const &view)
{
	if (focused_view() == &view) {
		Mode::forget(view);
		_menubar.state(*this, "", "", BLACK);
		update_all_views();
	}
	if (_input_receiver && view.belongs_to(*_input_receiver))
		_input_receiver = 0;

	if (_pointed_view == &view)
		_pointed_view = find_view(_mouse_pos);
}

