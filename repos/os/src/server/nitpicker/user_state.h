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

#include "mode.h"
#include "view_stack.h"
#include "global_keys.h"

class User_state : public Mode, public View_stack
{
	private:

		/*
		 * Policy for the routing of global keys
		 */
		Global_keys &_global_keys;

		/*
		 * Current pointer position
		 */
		Point _pointer_pos;

		/*
		 * Currently pointed-at session
		 */
		Session *_pointed_session = nullptr;

		/*
		 * Session that receives the current stream of input events
		 */
		Session *_input_receiver = nullptr;

		void _update_all();

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

	public:

		/**
		 * Constructor
		 */
		User_state(Global_keys &, Area view_stack_size);

		/**
		 * Handle input event
		 *
		 * This function controls the Nitpicker mode and the user state
		 * variables.
		 */
		void handle_event(Input::Event ev);

		void report_keystate(Genode::Xml_generator &);

		/**
		 * Accessors
		 */
		Point    pointer_pos()     { return _pointer_pos; }
		Session *pointed_session() { return _pointed_session; }


		/**
		 * (Re-)apply origin policy to all views
		 */
		void apply_origin_policy(View &pointer_origin)
		{
			View_stack::apply_origin_policy(pointer_origin);
		}

		/**
		 * Mode interface
		 */
		void forget(Session const &) override;
		void focused_session(Session *) override;
};

#endif
