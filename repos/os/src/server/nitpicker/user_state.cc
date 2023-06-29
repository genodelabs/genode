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

	bool const first_is_absolute_motion = ev->absolute_motion();
	bool const first_is_relative_motion = ev->relative_motion();

	unsigned cnt = 1;
	for (ev++ ; cnt < max; cnt++, ev++) {
		if (first_is_absolute_motion && ev->absolute_motion()) continue;
		if (first_is_relative_motion && ev->relative_motion()) continue;
		break;
	};
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
	if (n == 0) return Event();

	if (ev->relative_motion()) {
		int rx = 0, ry = 0;
		for (unsigned i = 0; i < n; i++, ev++)
			ev->handle_relative_motion([&] (int x, int y) { rx += x; ry += y; });

		if (rx || ry)
			return Relative_motion{rx, ry};
	}

	if (ev->absolute_motion()) {
		int ax = 0, ay = 0;
		for (unsigned i = 0; i < n; i++, ev++)
			ev->handle_absolute_motion([&] (int x, int y) { ax = x; ay = y; });

		return Absolute_motion{ax, ay};
	}

	return Event();
}


/**************************
 ** User state interface **
 **************************/

void User_state::_handle_input_event(Input::Event ev)
{
	ev.handle_seq_number([&] (Input::Seq_number const &seq) {
		_last_seq_number.construct(seq); });

	/* transparently convert relative into absolute motion event */
	ev.handle_relative_motion([&] (int x, int y) {

		int const ox = _pointer_pos.x(),
		          oy = _pointer_pos.y();

		int const ax = max(0, min((int)_view_stack.size().w() - 1, ox + x)),
		          ay = max(0, min((int)_view_stack.size().h() - 1, oy + y));

		ev = Absolute_motion{ax, ay};
	});

	/* respond to motion events by updating the pointer position */
	ev.handle_absolute_motion([&] (int x, int y) {
		_pointer_pos = Point(x, y); });

	/* let pointer position correspond to most recent touch position */
	ev.handle_touch([&] (Input::Touch_id, float x, float y) {
		_pointer_pos = Point((int)x, (int)y); });

	/* track key states, drop double press/release events */
	{
		bool drop = false;

		ev.handle_press([&] (Keycode key, Codepoint) {
			if (_key_array.pressed(key)) {
				warning("suspicious double press of ", Input::key_name(key));
				drop = true;
			}
			_key_array.pressed(key, true);
		});

		ev.handle_release([&] (Keycode key) {
			if (!_key_array.pressed(key)) {
				warning("suspicious double release of ", Input::key_name(key));
				drop = true;
			}
			_key_array.pressed(key, false);
		});

		if (drop)
			return;
	}

	/* count keys */
	if (ev.press()) _key_cnt++;
	if (ev.release() && (_key_cnt > 0)) _key_cnt--;

	if (ev.absolute_motion() || ev.relative_motion() || ev.touch()) {
		update_hover();

		if (_key_cnt > 0) {
			_drag = true;

			/*
			 * Submit leave event to the originally hovered client if motion
			 * occurs while a key is held. Otherwise, both the hovered client
			 * and the receiver of the key sequence would observe a motion
			 * event last, each appearing as being hovered at the same time.
			 */
			if (_hovered && (_input_receiver != _hovered)) {
				_hovered->submit_input_event(Hover_leave());
				_hovered = nullptr; /* updated when _key_cnt reaches 0 */
			}
		}
	}

	/*
	 * Handle start of a key sequence
	 */
	ev.handle_press([&] (Keycode keycode, Codepoint) {

		if (_key_cnt != 1)
			return;

		View_owner *global_receiver = nullptr;

		if (_mouse_button(keycode))
			_clicked_count++;

		_last_clicked = nullptr;

		/* update focused session */
		if (_mouse_button(keycode)
		 && _hovered
		 && (_hovered != _focused)
		 && (_hovered->has_focusable_domain()
		  || _hovered->has_same_domain(_focused))) {

			/*
			 * Notify both the old focused session and the new one.
			 */
			if (_focused)
				_focused->submit_input_event(Focus_leave());

			if (_hovered) {
				_hovered->submit_input_event(Absolute_motion{_pointer_pos.x(),
				                                             _pointer_pos.y()});
				_hovered->submit_input_event(Focus_enter());
			}

			if (_hovered->has_transient_focusable_domain()) {
				global_receiver = &_hovered->forwarded_focus();
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
					_focus_view_owner_via_click(_hovered->forwarded_focus());
				else
					global_receiver = &_hovered->forwarded_focus();

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
	});

	/*
	 * Deliver event to session
	 */
	bool const forward_to_session = (ev.absolute_motion() || ev.wheel() ||
	                                 ev.touch() || ev.touch_release() ||
	                                 ev.seq_number());
	if (forward_to_session) {

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
	ev.handle_press([&] (Keycode key, Codepoint) {

		if (!_input_receiver)
			return;

		if (!_mouse_button(key)
		 || (_hovered
		  && (_hovered->has_focusable_domain()
		   || _hovered->has_same_domain(_focused))))
			_input_receiver->submit_input_event(ev);
		else
			_input_receiver = nullptr;
	});

	if (ev.release() && _input_receiver)
		_input_receiver->submit_input_event(ev);

	/*
	 * Detect end of key sequence
	 */
	if (ev.release() && (_key_cnt == 0)) {

		update_hover();

		if (_drag && _input_receiver && (_input_receiver != _hovered))
			_input_receiver->submit_input_event(Hover_leave());

		_drag = false;

		if (_global_key_sequence) {
			_input_receiver      = _focused;
			_global_key_sequence = false;
		}
	}
}


User_state::Handle_input_result
User_state::handle_input_events(Input_batch batch)
{
	Point              const old_pointer_pos    = _pointer_pos;
	View_owner       * const old_hovered        = _hovered;
	View_owner const * const old_focused        = _focused;
	View_owner const * const old_input_receiver = _input_receiver;
	View_owner const * const old_last_clicked   = _last_clicked;
	unsigned           const old_clicked_count  = _clicked_count;

	bool button_activity = false;

	if (batch.count > 0) {
		/*
		 * Take events from input event buffer, merge consecutive motion
		 * events, and pass result to the user state.
		 */
		for (unsigned src_ev_cnt = 0; src_ev_cnt < batch.count; src_ev_cnt++) {

			Input::Event const *e = &batch.events[src_ev_cnt];
			Input::Event curr = *e;

			if (e->absolute_motion() || e->relative_motion()) {

				unsigned const n =
					num_consecutive_events(e, (unsigned)(batch.count - src_ev_cnt));

				curr = merge_motion_events(e, n);

				/* skip merged events */
				src_ev_cnt += n - 1;
			}

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
		_handle_input_event(Event());
	}

	/*
	 * If at least one key is kept pressed, we regard the user as active.
	 */
	button_activity |= _key_pressed();

	bool touch_occurred = false;
	bool key_state_affected = false;
	for (unsigned i = 0; i < batch.count; i++) {
		key_state_affected |= (batch.events[i].press() || batch.events[i].release());
		touch_occurred     |= batch.events[i].touch();
	}

	_apply_pending_focus_change();

	/*
	 * Determine condition for generating an updated "clicked" report
	 */
	bool const click_occurred = (old_clicked_count != _clicked_count);

	bool const clicked_report_up_to_date =
		(_last_clicked == old_last_clicked) && !_last_clicked_redeliver;

	bool const last_clicked_changed = (click_occurred && !clicked_report_up_to_date);

	if (last_clicked_changed) {
		_last_clicked_version++;
		_last_clicked_redeliver = false;
	}

	return {
		.hover_changed        = _hovered != old_hovered,
		.focus_changed        = (_focused != old_focused) ||
		                        (_input_receiver != old_input_receiver),
		.key_state_affected   = key_state_affected,
		.button_activity      = button_activity,
		.motion_activity      = (_pointer_pos != old_pointer_pos) || touch_occurred,
		.key_pressed          = _key_pressed(),
		.last_clicked_changed = last_clicked_changed
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
	if (_last_seq_number.constructed())
		xml.attribute("seq", _last_seq_number->value);

	if (_last_clicked)
		_last_clicked->report(xml);

	xml.attribute("version", _last_clicked_version);
}


User_state::Handle_forget_result User_state::forget(View_owner const &owner)
{
	_focus.forget(owner);

	bool const need_to_update_all_views = (&owner == _focused);
	bool const focus_vanished = (&owner == _focused);
	bool const hover_vanished = (&owner == _hovered);

	auto wipe_ptr = [&] (auto &ptr) {
		if (&owner == ptr)
			ptr = nullptr; };

	wipe_ptr(_focused);
	wipe_ptr(_next_focused);
	wipe_ptr(_last_clicked);
	wipe_ptr(_hovered);

	Update_hover_result const update_hover_result = update_hover();

	wipe_ptr(_input_receiver);

	if (need_to_update_all_views)
		_view_stack.update_all_views();

	return {
		.hover_changed = update_hover_result.hover_changed
		               || hover_vanished,
		.focus_changed = focus_vanished,
	};
}


User_state::Update_hover_result User_state::update_hover()
{
	/* no hover changes while dragging */
	if (_key_pressed())
		return { .hover_changed = false };

	View_owner * const old_hovered  = _hovered;
	View const * const pointed_view = _view_stack.find_view(_pointer_pos);

	_hovered = pointed_view ? &pointed_view->owner() : nullptr;

	/*
	 * Deliver a leave event if pointed-to session changed, notify newly
	 * hovered session about the current pointer position.
	 */
	if (old_hovered != _hovered) {
		if (old_hovered)
			old_hovered->submit_input_event(Hover_leave());

		if (_hovered)
			_hovered->submit_input_event(Absolute_motion{_pointer_pos.x(),
			                                             _pointer_pos.y()});
	}

	return { .hover_changed = (_hovered != old_hovered) };
}


void User_state::_focus_view_owner_via_click(View_owner &owner)
{
	_next_focused = &owner;
	_focused      = &owner;

	_focus.assign(owner);

	if (!_global_key_sequence)
		_input_receiver = &owner;
}
