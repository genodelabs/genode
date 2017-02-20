/*
 * \brief   User event representation
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__EVENT_H_
#define _INCLUDE__SCOUT__EVENT_H_

#include <scout/types.h>

namespace Scout {
	class Event;
	class Event_handler;
}


/**
 * Event structure
 *
 * This event structure covers timer events as well as user input events.
 */
class Scout::Event
{
	public:

		/**
		 * Some sensibly choosen key and button codes
		 */
		enum code {
			BTN_LEFT = 0x110,
			KEY_Q    = 16,
		};

		enum ev_type {
			UNDEFINED = 0,
			MOTION    = 1,   /* mouse moved         */
			PRESS     = 2,   /* button/key pressed  */
			RELEASE   = 3,   /* button/key released */
			TIMER     = 4,   /* timer event         */
			QUIT      = 5,   /* quit application    */
			WHEEL     = 6,   /* mouse wheel         */
		};

		ev_type type;
		Point   mouse_position;
		Point   wheel_movement;
		int     code;       /* key code */

		/**
		 * Assign new event information to event structure
		 */
		inline void assign(ev_type new_type, int new_mx, int new_my, int new_code)
		{
			type           = new_type;
			mouse_position = Point(new_mx, new_my);
			wheel_movement = Point(0, 0);
			code           = new_code;
		}
};


class Scout::Event_handler
{
	public:

		virtual ~Event_handler() { }

		/**
		 * Handle event
		 */
		virtual void handle_event(Event const &e) = 0;
};

#endif /* _INCLUDE__SCOUT__EVENT_H_ */
