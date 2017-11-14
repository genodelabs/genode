/*
 * \brief  Input event structure
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__INPUT__EVENT_H_
#define _INCLUDE__INPUT__EVENT_H_

#include <input/keycodes.h>

namespace Input { class Event; }


class Input::Event
{
	public:

		enum Type { INVALID, MOTION, PRESS, RELEASE, WHEEL, FOCUS, LEAVE, TOUCH,
		            CHARACTER };

	private:

		Type _type = INVALID;

		/*
		 * For PRESS and RELEASE events, '_code' contains the key code.
		 * For FOCUS events, '_code' is set to 1 (focus) or 0 (unfocus).
		 */
		int _code = 0;

		/*
		 * Absolute pointer position coordinates
		 */
		int _ax = 0, _ay = 0;

		/*
		 * Relative pointer motion vector
		 */
		int _rx = 0, _ry = 0;

	public:

		/**
		 * UTF8-encoded symbolic character
		 */
		struct Utf8
		{
			typedef unsigned char byte;

			byte b0, b1, b2, b3;

			Utf8(byte b0, byte b1 = 0, byte b2 = 0, byte b3 = 0)
			: b0(b0), b1(b1), b2(b2), b3(b3) { }
		};

		/**
		 * Default constructor creates an invalid event
		 */
		Event() { }

		/**
		 * Constructor creates a low-level event
		 */
		Event(Type type, int code, int ax, int ay, int rx, int ry):
			_type(type), _code(code),
			_ax(ax), _ay(ay), _rx(rx), _ry(ry) { }

		/**
		 * Constructor creates a symbolic character event
		 */
		Event(Utf8 const &utf8)
		:
			_type(CHARACTER),
			_code(((unsigned)utf8.b3 << 24) | ((unsigned)utf8.b2 << 16) |
			      ((unsigned)utf8.b1 <<  8) | ((unsigned)utf8.b0 <<  0))
		{ }

		/**
		 * Return event type
		 */
		Type type() const
		{
			/* prevent obnoxious events from being processed by clients */
			if ((_type == PRESS || _type == RELEASE)
			 && (_code <= KEY_RESERVED || _code >= KEY_UNKNOWN))
				return INVALID;

			return _type;
		}

		/**
		 * Accessors
		 */
		int  code() const { return _code; }
		int  ax()   const { return _ax; }
		int  ay()   const { return _ay; }
		int  rx()   const { return _rx; }
		int  ry()   const { return _ry; }

		/**
		 * Return symbolic character encoded as UTF8 byte sequence
		 *
		 * This method must only be called if type is CHARACTER.
		 */
		Utf8 utf8() const { return Utf8(_code, _code >> 8, _code >> 16, _code >> 24); }

		/**
		 * Return key code for press/release events
		 */
		Keycode keycode() const
		{
			return _type == PRESS || _type == RELEASE ? (Keycode)_code : KEY_UNKNOWN;
		}

		bool absolute_motion() const { return _type == MOTION && !_rx && !_ry; }
		bool relative_motion() const { return _type == MOTION && (_rx || _ry); }
		bool touch_release()   const { return _type == TOUCH && (_rx == -1) && (_ry == -1); }

		/*
		 * \deprecated use methods without the 'is_' prefix instead
		 */
		bool is_absolute_motion() const { return absolute_motion(); }
		bool is_relative_motion() const { return relative_motion(); }
		bool is_touch_release()   const { return touch_release(); }

		static Event create_touch_event(int ax, int ay, int id, bool last = false)
		{
			return Event(Type::TOUCH, id, ax, ay, last ? -1 : 0, last ? -1 : 0);
		}
};

#endif /* _INCLUDE__INPUT__EVENT_H_ */
