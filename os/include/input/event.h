/*
 * \brief  Input event structure
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INPUT__EVENT_H_
#define _INCLUDE__INPUT__EVENT_H_

namespace Input {

	class Event
	{
		public:

			enum Type { INVALID, MOTION, PRESS, RELEASE, WHEEL, LEAVE };

		private:

			Type _type;
			int  _keycode;
			int  _ax, _ay;
			int  _rx, _ry;

		public:

			/**
			 * Constructors
			 */
			Event():
				_type(INVALID), _keycode(0), _ax(0), _ay(0), _rx(0), _ry(0) { }

			Event(Type type, int keycode, int ax, int ay, int rx, int ry):
				_type(type), _keycode(keycode),
				_ax(ax), _ay(ay), _rx(rx), _ry(ry) { }

			/**
			 * Accessors
			 */
			Type type()    { return _type; }
			int  keycode() { return _keycode; }
			int  ax()      { return _ax; }
			int  ay()      { return _ay; }
			int  rx()      { return _rx; }
			int  ry()      { return _ry; }

			bool is_absolute_motion() { return _type == MOTION && !_rx && !_ry; }
			bool is_relative_motion() { return _type == MOTION && (_rx || _ry); }
	};
}

#endif /* _INCLUDE__INPUT__EVENT_H_ */
