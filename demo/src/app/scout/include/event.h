/*
 * \brief   User event representation
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EVENT_H_
#define _EVENT_H_

/**
 * Event structure
 *
 * This event structure covers timer events as
 * well as user input events.
 */
class Event
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
			REFRESH   = 6,   /* refresh screen      */
			WHEEL     = 7,   /* mouse wheel         */
		};

		ev_type  type;
		int      mx, my;     /* mouse position */
		int      wx, wy;     /* wheel          */
		int      code;       /* key code       */

		/**
		 * Assign new event information to event structure
		 */
		inline void assign(ev_type new_type, int new_mx, int new_my, int new_code)
		{
			type = new_type;
			mx   = new_mx;
			my   = new_my;
			wx   = 0;
			wy   = 0;
			code = new_code;
		}
};


/**
 * Event handler
 */
class Event_handler
{
	public:

		virtual ~Event_handler() { }

		/**
		 * Handle event
		 */
		virtual void handle(Event &e) = 0;
};


#endif /* _EVENT_H_ */
