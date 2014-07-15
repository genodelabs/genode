/*
 * \brief  Input event structure
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INPUT__EVENT_H_
#define _INCLUDE__INPUT__EVENT_H_

#include <input/keycodes.h>

namespace Input {

	class Event
	{
		public:

			enum Type { INVALID, MOTION, PRESS, RELEASE, WHEEL, FOCUS, LEAVE };

		private:

			Type _type;

			/*
			 * For PRESS and RELEASE events, '_code' contains the key code.
			 * For FOCUS events, '_code' is set to 1 (focus) or 0 (unfocus).
			 */
			int _code;

			/*
			 * Absolute pointer position coordinates
			 */
			int _ax, _ay;

			/*
			 * Relative pointer motion vector
			 */
			int _rx, _ry;

		public:

			/**
			 * Constructors
			 */
			Event():
				_type(INVALID), _code(0), _ax(-1), _ay(-1), _rx(0), _ry(0) { }

			Event(Type type, int code, int ax, int ay, int rx, int ry):
				_type(type), _code(code),
				_ax(ax), _ay(ay), _rx(rx), _ry(ry) { }

			/**
			 * Accessors
			 */
			Type type() const { return _type; }
			int  code() const { return _code; }
			int  ax()   const { return _ax; }
			int  ay()   const { return _ay; }
			int  rx()   const { return _rx; }
			int  ry()   const { return _ry; }

			/**
			 * Return key code for press/release events
			 */
			Keycode keycode() const
			{
				return _type == PRESS || _type == RELEASE ? (Keycode)_code : KEY_UNKNOWN;
			}

			bool is_absolute_motion() const { return _type == MOTION && !_rx && !_ry; }
			bool is_relative_motion() const { return _type == MOTION && (_rx || _ry); }
	};
}

#endif /* _INCLUDE__INPUT__EVENT_H_ */
