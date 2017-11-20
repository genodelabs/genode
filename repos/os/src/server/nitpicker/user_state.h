/*
 * \brief  Nitpicker user state handling
 * \author Norman Feske
 * \date   2006-08-09
 *
 * This class comprehends the policy of user interaction.
 * It manages the toggling of Nitpicker's different modes
 * and routes input events to corresponding client sessions.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _USER_STATE_H_
#define _USER_STATE_H_

#include <util/xml_generator.h>

#include "focus.h"
#include "view_stack.h"
#include "global_keys.h"

namespace Nitpicker { class User_state; }


class Nitpicker::User_state : public Focus_controller
{
	private:

		/*
		 * Number of currently pressed keys. This counter is used to determine
		 * if the user is dragging an item.
		 */
		unsigned _key_cnt = 0;

		View_owner *_focused = nullptr;

		View_owner *_next_focused = nullptr;

		/*
		 * True while a global key sequence is processed
		 */
		bool _global_key_sequence = false;

		/*
		 * True if the input focus should change directly whenever the user
		 * clicks on an unfocused client. This is the traditional behaviour
		 * of nitpicker. This builtin policy is now superseded by the use of an
		 * external focus-management component (e.g., nit_focus).
		 */
		bool _focus_via_click = true;

		/**
		 * Input-focus information propagated to the view stack
		 */
		Focus &_focus;

		/**
		 * Policy for the routing of global keys
		 */
		Global_keys &_global_keys;

		/**
		 * View stack, used to determine the hovered view and pointer boundary
		 */
		View_stack &_view_stack;

		/*
		 * Current pointer position
		 */
		Point _pointer_pos;

		/*
		 * Currently pointed-at view owner
		 */
		View_owner *_hovered = nullptr;

		/*
		 * View owner that receives the current stream of input events
		 */
		View_owner *_input_receiver = nullptr;

		/**
		 * View owner that was last clicked-on by the user
		 */
		View_owner *_last_clicked = nullptr;

		/**
		 * Array for tracking the state of each key
		 */
		struct Key_array
		{
			struct State { bool pressed = false; };

			State _states[Input::KEY_MAX + 1];

			void pressed(Input::Keycode key, bool pressed)
			{
				if (key <= Input::KEY_MAX)
					_states[key].pressed = pressed;
			}

			bool pressed(Input::Keycode key) const
			{
				return (key <= Input::KEY_MAX) && _states[key].pressed;
			}

			void report_state(Genode::Xml_generator &xml) const
			{
				for (unsigned i = 0; i <= Input::KEY_MAX; i++)
					if (_states[i].pressed)
						xml.node("pressed", [&] () {
							xml.attribute("key", Input::key_name((Input::Keycode)i)); });
			}

		} _key_array;

		bool _focus_change_permitted(View_owner const &caller) const;

		void _focus_view_owner_via_click(View_owner &);

		void _handle_input_event(Input::Event);

		bool _key_pressed() const { return _key_cnt > 0; }

		/**
		 * Apply pending focus-change request that was issued during drag state
		 */
		void _apply_pending_focus_change()
		{
			/*
			 * Defer focus changes to a point where no drag operation is in
			 * flight because otherwise, the involved sessions would obtain
			 * inconsistent press and release events. However, focus changes
			 * during global key sequences are fine.
			 */
			if (_key_pressed() && !_global_key_sequence)
				return;

			if (_focused != _next_focused) {
				_focused  = _next_focused;

				/* propagate changed focus to view stack */
				if (_focused)
					_focus.assign(*_focused);
				else
					_focus.reset();
			}
		}

	public:

		/**
		 * Constructor
		 *
		 * \param focus  exported focus information, to be consumed by the
		 *               view stack to tailor its view drawing operations
		 */
		User_state(Focus &focus, Global_keys &global_keys, View_stack &view_stack)
		:
			_focus(focus), _global_keys(global_keys), _view_stack(view_stack)
		{ }


		/********************************
		 ** Focus_controller interface **
		 ********************************/

		void focus_view_owner(View_owner const &caller,
		                      View_owner &next_focused) override
		{
			/* check permission by comparing session labels */
			if (!_focus_change_permitted(caller)) {
				warning("unauthorized focus change requesed by ", caller.label());
				return;
			}

			/*
			 * To avoid changing the focus in the middle of a drag operation,
			 * we cannot perform the focus change immediately. Instead, it
			 * comes into effect via the 'apply_pending_focus_change()' method
			 * called the next time when the user input is handled and no drag
			 * operation is in flight.
			 */
			_next_focused = &next_focused;
		}


		/****************************************
		 ** Interface used by the main program **
		 ****************************************/

		struct Handle_input_result
		{
			bool const hover_changed;
			bool const focus_changed;
			bool const key_state_affected;
			bool const button_activity;
			bool const motion_activity;
			bool const key_pressed;
			bool const last_clicked_changed;
		};

		Handle_input_result handle_input_events(Input::Event const *ev_buf,
		                                        unsigned num_ev);

		/**
		 * Discard all references to specified view owner
		 */
		void forget(View_owner const &);

		void report_keystate(Xml_generator &) const;
		void report_pointer_position(Xml_generator &) const;
		void report_hovered_view_owner(Xml_generator &, bool motion_active) const;
		void report_focused_view_owner(Xml_generator &, bool button_active) const;
		void report_last_clicked_view_owner(Xml_generator &) const;

		Point pointer_pos() { return _pointer_pos; }

		/**
		 * Enable/disable direct focus changes by clicking on a client
		 */
		void focus_via_click(bool enabled) { _focus_via_click = enabled; }

		/**
		 * Set input focus to specified view owner
		 *
		 * This method is used when nitpicker's focus is managed by an
		 * external focus-policy component like 'nit_focus'.
		 *
		 * The focus change is not applied immediately but deferred to the
		 * next call of 'handle_input_events' (which happens periodically).
		 */
		void focus(View_owner &owner)
		{
			/*
			 * The focus change is not applied immediately but deferred to the
			 * next call of '_apply_pending_focus_change' via the periodic
			 * call of 'handle_input_events'.
			 */
			_next_focused = &owner;
		}

		void reset_focus() { _next_focused = nullptr; }
};

#endif /* _USER_STATE_H_ */
