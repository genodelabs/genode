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

User_state::User_state(Global_keys &global_keys, Area view_stack_size)
:
	View_stack(view_stack_size, *this), _global_keys(global_keys)
{ }


void User_state::_update_all()
{
	update_all_views();
}


void User_state::handle_event(Input::Event ev)
{
	Input::Keycode     const keycode = ev.keycode();
	Input::Event::Type const type    = ev.type();

	/*
	 * Mangle incoming events
	 */
	int ax = _pointer_pos.x(), ay = _pointer_pos.y();
	int rx = 0, ry = 0; /* skip info about relative motion by default */

	/* transparently handle absolute and relative motion events */
	if (type == Event::MOTION) {
		if ((ev.rx() || ev.ry()) && ev.ax() == 0 && ev.ay() == 0) {
			ax = Genode::max(0, Genode::min((int)size().w(), ax + ev.rx()));
			ay = Genode::max(0, Genode::min((int)size().h(), ay + ev.ry()));
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

	if (type == Event::TOUCH) {
		ax = ev.ax();
		ay = ev.ay();
		ev = Input::Event::create_touch_event(ax, ay, ev.code(),
		                                      ev.is_touch_release());
	} else
		ev = Input::Event(type, keycode, ax, ay, rx, ry);

	_pointer_pos = Point(ax, ay);

	/* count keys */
	if (type == Event::PRESS)                   Mode::inc_key_cnt();
	if (type == Event::RELEASE && Mode::drag()) Mode::dec_key_cnt();

	View const * const pointed_view    = find_view(_pointer_pos);
	::Session  * const pointed_session = pointed_view ? &pointed_view->session() : 0;

	/*
	 * Deliver a leave event if pointed-to session changed
	 */
	if (_pointed_session && (pointed_session != _pointed_session)) {

		Input::Event leave_ev(Input::Event::LEAVE, 0, ax, ay, 0, 0);
		_pointed_session->submit_input_event(leave_ev);
	}

	_pointed_session = pointed_session;

	/**
	 * Guard that, when 'enabled' is set to true, performs a whole-screen
	 * update when leaving the scope
	 */
	struct Update_all_guard
	{
		User_state  &user_state;
		bool        update = false;

		Update_all_guard(User_state &user_state)
		: user_state(user_state) { }

		~Update_all_guard()
		{
			if (update)
				user_state._update_all();
		}
	} update_all_guard(*this);

	/*
	 * Handle start of a key sequence
	 */
	if (type == Event::PRESS && Mode::has_key_cnt(1)) {

		/*
		 * Detect mouse press event in kill mode, used to select the session
		 * to lock out.
		 */
		if (kill() && keycode == Input::BTN_LEFT) {
			if (_pointed_session)
				lock_out_session(*_pointed_session);

			/* leave kill mode */
			update_all_guard.update = true;
			Mode::leave_kill();
			return;
		}

		/* update focused session */
		if (pointed_session != Mode::focused_session() && _mouse_button(keycode)) {

			update_all_guard.update = true;

			/*
			 * Notify both the old focused session and the new one.
			 */
			if (Mode::focused_session()) {
				Input::Event unfocus_ev(Input::Event::FOCUS, 0, ax, ay, 0, 0);
				Mode::focused_session()->submit_input_event(unfocus_ev);
			}

			if (_pointed_session) {
				Input::Event focus_ev(Input::Event::FOCUS, 1, ax, ay, 0, 0);
				pointed_session->submit_input_event(focus_ev);
			}

			focused_session(_pointed_session);
		}

		/*
		 * If there exists a global rule for the pressed key, set the
		 * corresponding session as receiver of the input stream until the key
		 * count reaches zero. Otherwise, the input stream is directed to the
		 * pointed-at session.
		 *
		 * If we deliver a global key sequence, we temporarily change the focus
		 * to the global receiver. To reflect that change, we need to update
		 * the whole screen.
		 */
		::Session * const global_receiver = _global_keys.global_receiver(keycode);
		if (global_receiver) {
			_global_key_sequence    = true;
			_input_receiver         = global_receiver;
			update_all_guard.update = true;
		}

		/*
		 * No global rule matched, so the input stream gets directed to the
		 * focused session or refers to a built-in operation.
		 */
		if (!global_receiver) {
			_input_receiver = Mode::focused_session();
		}

		/*
		 * Toggle kill and xray modes. If one of those keys is pressed,
		 * suppress the delivery to clients.
		 */
		if (_global_keys.is_operation_key(keycode)) {

			if (_global_keys.is_kill_key(keycode)) {
				Mode::toggle_kill();
				_input_receiver = 0;
			}

			if (_global_keys.is_xray_key(keycode)) Mode::toggle_xray();

			update_all_guard.update = true;
		}
	}

	/*
	 * Deliver event to session except when kill mode is activated
	 */
	if (kill()) return;

	if (type == Event::MOTION || type == Event::WHEEL || type == Event::TOUCH) {

		if (Mode::has_key_cnt(0)) {

			/*
			 * In flat mode, we deliver motion events to the pointed-at
			 * session. In xray mode, we deliver motion events only to the
			 * focused session.
			 */
			if (flat() || (xray() && Mode::focused_session() == pointed_session)
			 || (pointed_session && pointed_session->xray_no()))
				if (pointed_session)
					pointed_session->submit_input_event(ev);

		} else if (_input_receiver)
			_input_receiver->submit_input_event(ev);
	}

	/*
	 * Deliver press/release event to focused session. Never deliver events
	 * for keys that are configured for global operations.
	 */
	if (type == Event::PRESS || type == Event::RELEASE)
		if (_input_receiver && !_global_keys.is_operation_key(keycode))
			_input_receiver->submit_input_event(ev);

	/*
	 * Detect end of global key sequence
	 */
	if (ev.type() == Event::RELEASE && Mode::has_key_cnt(0) && _global_key_sequence) {

		_input_receiver = Mode::focused_session();

		update_all_guard.update = true;

		_global_key_sequence = false;
	}
}


/********************
 ** Mode interface **
 ********************/

void User_state::forget(::Session const &session)
{
	Mode::forget(session);

	if (_pointed_session == &session) {
		View * const pointed_view = find_view(_pointer_pos);
		_pointed_session = pointed_view ? &pointed_view->session() : nullptr;
	}

	if (_input_receiver == &session)
		_input_receiver = nullptr;
}


void User_state::focused_session(::Session *session)
{
	Mode::focused_session(session);

	if (!_global_key_sequence)
		_input_receiver = session;
}
