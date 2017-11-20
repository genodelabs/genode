/*
 * \brief  User state implementation
 * \author Norman Feske
 * \date   2006-08-27
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <input/event.h>
#include <input/keycodes.h>

#include "user_state.h"

using namespace Input;


/***************
 ** Utilities **
 ***************/

static inline bool _mouse_button(Keycode keycode) {
		return keycode >= BTN_LEFT && keycode <= BTN_MIDDLE; }


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

	bool const first_absolute = ev->absolute_motion();

	/* iterate until we get a different event type, start at second */
	unsigned cnt = 1;
	for (ev++ ; cnt < max; cnt++, ev++) {
		if (ev->type() != Input::Event::MOTION) break;
		if (first_absolute != ev->absolute_motion()) break;
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
		res = Input::Event(Input::Event::MOTION, 0, ev->ax(), ev->ay(),
		                   res.rx() + ev->rx(), res.ry() + ev->ry());
	return res;
}


/**************************
 ** User state interface **
 **************************/

void User_state::_handle_input_event(Input::Event ev)
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
			ax = max(0, min((int)_view_stack.size().w() - 1, ax + ev.rx()));
			ay = max(0, min((int)_view_stack.size().h() - 1, ay + ev.ry()));
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
		                                      ev.touch_release());
	} else if (type == Event::CHARACTER) {
		ev = Input::Event(type, ev.code(), ax, ay, rx, ry);
	} else
		ev = Input::Event(type, keycode, ax, ay, rx, ry);

	_pointer_pos = Point(ax, ay);

	bool const drag = _key_cnt > 0;

	/* count keys */
	if (type == Event::PRESS)           _key_cnt++;
	if (type == Event::RELEASE && drag) _key_cnt--;

	/* track key states */
	if (type == Event::PRESS) {
		if (_key_array.pressed(keycode))
			Genode::warning("suspicious double press of ", Input::key_name(keycode));
		_key_array.pressed(keycode, true);
	}

	if (type == Event::RELEASE) {
		if (!_key_array.pressed(keycode))
			Genode::warning("suspicious double release of ", Input::key_name(keycode));
		_key_array.pressed(keycode, false);
	}

	View_component const * const pointed_view = _view_stack.find_view(_pointer_pos);

	View_owner * const hovered = pointed_view ? &pointed_view->owner() : 0;

	/*
	 * Deliver a leave event if pointed-to session changed
	 */
	if (_hovered && (hovered != _hovered)) {

		Input::Event leave_ev(Input::Event::LEAVE, 0, ax, ay, 0, 0);
		_hovered->submit_input_event(leave_ev);
	}

	_hovered = hovered;

	/*
	 * Handle start of a key sequence
	 */
	if (type == Event::PRESS && (_key_cnt == 1)) {

		View_owner *global_receiver = nullptr;

		/* update focused session */
		if (_mouse_button(keycode)
		 && _hovered
		 && (_hovered != _focused)
		 && (_hovered->has_focusable_domain()
		  || _hovered->has_same_domain(_focused))) {

			/*
			 * Notify both the old focused session and the new one.
			 */
			if (_focused) {
				Input::Event unfocus_ev(Input::Event::FOCUS, 0, ax, ay, 0, 0);
				_focused->submit_input_event(unfocus_ev);
			}

			if (_hovered) {
				Input::Event focus_ev(Input::Event::FOCUS, 1, ax, ay, 0, 0);
				_hovered->submit_input_event(focus_ev);
			}

			if (_hovered->has_transient_focusable_domain()) {
				global_receiver = _hovered;
			} else {
				/*
				 * Distinguish the use of the builtin focus switching and the
				 * alternative use of an external focus policy. In the latter
				 * case, focusable domains are handled like transiently
				 * focusable domains. The permanent focus change is triggered
				 * by an external focus-policy component by posting and updated
				 * focus ROM, which is then propagated into the user state via
				 * the 'User_state::focus' and 'User_state::reset_focus'
				 * methods.
				 */
				if (_focus_via_click)
					_focus_view_owner_via_click(*_hovered);
				else
					global_receiver = _hovered;

				_last_clicked = _hovered;
			}
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
		if (!global_receiver)
			global_receiver = _global_keys.global_receiver(keycode);

		if (global_receiver) {
			_global_key_sequence = true;
			_input_receiver      = global_receiver;
		}

		/*
		 * No global rule matched, so the input stream gets directed to the
		 * focused session or refers to a built-in operation.
		 */
		if (!global_receiver)
			_input_receiver = _focused;
	}

	/*
	 * Deliver event to session
	 */
	if (type == Event::MOTION || type == Event::WHEEL || type == Event::TOUCH) {

		if (_key_cnt == 0) {

			if (_hovered) {

				/*
				 * Unless the domain of the pointed session is configured to
				 * always receive hover events, we deliver motion events only
				 * to the focused domain.
				 */
				if (_hovered->hover_always()
				 || _hovered->has_same_domain(_focused))
					_hovered->submit_input_event(ev);
			}

		} else if (_input_receiver)
			_input_receiver->submit_input_event(ev);
	}

	/*
	 * Deliver press/release event to focused session or the receiver of global
	 * key.
	 */
	if ((type == Event::PRESS) && _input_receiver) {
		if (!_mouse_button(ev.keycode())
		 || (_hovered
		  && (_hovered->has_focusable_domain()
		   || _hovered->has_same_domain(_focused))))
			_input_receiver->submit_input_event(ev);
		else
			_input_receiver = nullptr;
	}

	if ((type == Event::RELEASE) && _input_receiver)
		_input_receiver->submit_input_event(ev);

	/*
	 * Forward character events
	 */
	if (type == Event::CHARACTER && _input_receiver)
		_input_receiver->submit_input_event(ev);

	/*
	 * Detect end of global key sequence
	 */
	if (ev.type() == Event::RELEASE && (_key_cnt == 0) && _global_key_sequence) {
		_input_receiver = _focused;
		_global_key_sequence = false;
	}
}


User_state::Handle_input_result
User_state::handle_input_events(Input::Event const * const ev_buf,
                                unsigned const num_ev)
{
	Point              const old_pointer_pos    = _pointer_pos;
	View_owner       * const old_hovered        = _hovered;
	View_owner const * const old_focused        = _focused;
	View_owner const * const old_input_receiver = _input_receiver;
	View_owner const * const old_last_clicked   = _last_clicked;

	bool button_activity = false;

	if (num_ev > 0) {
		/*
		 * Take events from input event buffer, merge consecutive motion
		 * events, and pass result to the user state.
		 */
		for (unsigned src_ev_cnt = 0; src_ev_cnt < num_ev; src_ev_cnt++) {

			Input::Event const *e = &ev_buf[src_ev_cnt];
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
			if (e->relative_motion() && curr.rx() == 0 && curr.ry() == 0)
				continue;

			/*
			 * If we detect a pressed key sometime during the event processing,
			 * we regard the user as active. This check captures the presence
			 * of press-release combinations within one batch of input events.
			 */
			button_activity |= _key_pressed();

			/* pass event to user state */
			_handle_input_event(curr);
		}
	} else {
		/*
		 * Besides handling input events, 'user_state.handle_event()' also
		 * updates the pointed session, which might have changed by other
		 * means, for example view movement.
		 */
		_handle_input_event(Input::Event());
	}

	/*
	 * If at least one key is kept pressed, we regard the user as active.
	 */
	button_activity |= _key_pressed();

	bool key_state_affected = false;
	for (unsigned i = 0; i < num_ev; i++)
		key_state_affected |= (ev_buf[i].type() == Input::Event::PRESS) ||
		                      (ev_buf[i].type() == Input::Event::RELEASE);

	_apply_pending_focus_change();

	return {
		.hover_changed        = _hovered != old_hovered,
		.focus_changed        = (_focused != old_focused) ||
		                        (_input_receiver != old_input_receiver),
		.key_state_affected   = key_state_affected,
		.button_activity      = button_activity,
		.motion_activity      = (_pointer_pos != old_pointer_pos),
		.key_pressed          = _key_pressed(),
		.last_clicked_changed = (_last_clicked != old_last_clicked)
	};
}


void User_state::report_keystate(Xml_generator &xml) const
{
	xml.attribute("count", _key_cnt);
	_key_array.report_state(xml);
}


void User_state::report_pointer_position(Xml_generator &xml) const
{
	xml.attribute("xpos", _pointer_pos.x());
	xml.attribute("ypos", _pointer_pos.y());
}


void User_state::report_hovered_view_owner(Xml_generator &xml, bool active) const
{
	if (_hovered)
		_hovered->report(xml);

	if (active) xml.attribute("active", "yes");
}


void User_state::report_focused_view_owner(Xml_generator &xml, bool active) const
{
	if (_focused) {
		_focused->report(xml);

		if (active) xml.attribute("active", "yes");
	}
}


void User_state::report_last_clicked_view_owner(Xml_generator &xml) const
{
	if (_last_clicked)
		_last_clicked->report(xml);
}


void User_state::forget(View_owner const &owner)
{
	_focus.forget(owner);

	if (&owner == _focused)      _focused      = nullptr;
	if (&owner == _next_focused) _next_focused = nullptr;
	if (&owner == _last_clicked) _last_clicked = nullptr;

	if (_hovered == &owner) {
		View_component * const pointed_view = _view_stack.find_view(_pointer_pos);
		_hovered = pointed_view ? &pointed_view->owner() : nullptr;
	}

	if (_input_receiver == &owner)
		_input_receiver = nullptr;
}


bool User_state::_focus_change_permitted(View_owner const &caller) const
{
	/*
	 * If no session is focused, we allow any client to assign it. This
	 * is useful for programs such as an initial login window that
	 * should receive input events without prior manual selection via
	 * the mouse.
	 *
	 * In principle, a client could steal the focus during time between
	 * a currently focused session gets closed and before the user
	 * manually picks a new session. However, in practice, the focus
	 * policy during application startup and exit is managed by a
	 * window manager that sits between nitpicker and the application.
	 */
	if (!_focused)
		return true;

	/*
	 * Check if the currently focused session label belongs to a
	 * session subordinated to the caller, i.e., it originated from
	 * a child of the caller or from the same process. This is the
	 * case if the first part of the focused session label is
	 * identical to the caller's label.
	 */
	char const * const focused_label = _focused->label().string();
	char const * const caller_label  = caller.label().string();

	return strcmp(focused_label, caller_label, strlen(caller_label)) == 0;
}


void User_state::_focus_view_owner_via_click(View_owner &owner)
{
	_next_focused = &owner;
	_focused      = &owner;

	_focus.assign(owner);

	if (!_global_key_sequence)
		_input_receiver = &owner;
}
