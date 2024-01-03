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

#include <base/output.h>
#include <input/keycodes.h>
#include <util/geometry.h>
#include <util/utf8.h>

namespace Input {

	typedef Genode::Codepoint Codepoint;

	struct Touch_id { unsigned value; };

	/*
	 * Event attributes
	 */
	struct Press           { Keycode key; };
	struct Press_char      { Keycode key; Codepoint codepoint; };
	struct Release         { Keycode key; };
	struct Wheel           { int x, y; };
	struct Focus_enter     { };
	struct Focus_leave     { };
	struct Hover_leave     { };
	struct Absolute_motion { int x, y; };
	struct Relative_motion { int x, y; };
	struct Touch           { Touch_id id; float x, y; };
	struct Touch_release   { Touch_id id; };
	struct Seq_number      { unsigned value; };

	struct Axis
	{
		enum class Id : unsigned { LX = 1, LY, LT, RX, RY, RT } id;

		float value;
	};

	class Event;
	class Binding;
}


class Input::Event
{
	private:

		enum Type { INVALID, PRESS, RELEASE, REL_MOTION, ABS_MOTION, WHEEL,
		            FOCUS_ENTER, FOCUS_LEAVE, HOVER_LEAVE, TOUCH, TOUCH_RELEASE,
		            SEQ_NUMBER, AXIS };

		Type _type = INVALID;

		struct Attr
		{
			union {
				Press_char      press;
				Release         release;
				Wheel           wheel;
				Absolute_motion abs_motion;
				Relative_motion rel_motion;
				Touch           touch;
				Touch_release   touch_release;
				Seq_number      seq_number;
				Axis            axis;
			};
		} _attr { };

		static bool _valid(Keycode key) { return key > KEY_RESERVED && key < KEY_MAX; }

		/**
		 * Return point representation of attribute 'a'
		 */
		template <typename R, typename T>
		static Genode::Point<R> _xy(T const &a) { return Genode::Point<R>(a.x, a.y); }

		friend class Input::Binding;

	public:

		/**
		 * Default constructor creates an invalid event
		 *
		 * This constructor can be used for creating an array of events.
		 */
		Event() { }

		/*
		 * Constructors for creating input events of various types
		 *
		 * These constructors can be used implicitly for the given attribute
		 * types for assignments or for passing an event as return value.
		 */
		Event(Press_char      arg) : _type(PRESS)         { _attr.press = arg; }
		Event(Press           arg) : Event(Press_char{arg.key, Codepoint{Codepoint::INVALID}}) { }
		Event(Release         arg) : _type(RELEASE)       { _attr.release = arg; }
		Event(Relative_motion arg) : _type(REL_MOTION)    { _attr.rel_motion = arg; }
		Event(Absolute_motion arg) : _type(ABS_MOTION)    { _attr.abs_motion = arg; }
		Event(Wheel           arg) : _type(WHEEL)         { _attr.wheel = arg; }
		Event(Focus_enter)         : _type(FOCUS_ENTER)   { }
		Event(Focus_leave)         : _type(FOCUS_LEAVE)   { }
		Event(Hover_leave)         : _type(HOVER_LEAVE)   { }
		Event(Touch           arg) : _type(TOUCH)         { _attr.touch = arg; }
		Event(Touch_release   arg) : _type(TOUCH_RELEASE) { _attr.touch_release = arg; }
		Event(Seq_number      arg) : _type(SEQ_NUMBER)    { _attr.seq_number = arg; }
		Event(Axis            arg) : _type(AXIS)          { _attr.axis = arg; }


		/************************************
		 ** Methods for handling the event **
		 ************************************/

		bool valid()           const { return _type != INVALID;       }
		bool press()           const { return _type == PRESS;         }
		bool release()         const { return _type == RELEASE;       }
		bool absolute_motion() const { return _type == ABS_MOTION;    }
		bool relative_motion() const { return _type == REL_MOTION;    }
		bool wheel()           const { return _type == WHEEL;         }
		bool focus_enter()     const { return _type == FOCUS_ENTER;   }
		bool focus_leave()     const { return _type == FOCUS_LEAVE;   }
		bool hover_leave()     const { return _type == HOVER_LEAVE;   }
		bool touch()           const { return _type == TOUCH;         }
		bool touch_release()   const { return _type == TOUCH_RELEASE; }
		bool seq_number()      const { return _type == SEQ_NUMBER;    }
		bool axis()            const { return _type == AXIS;          }

		bool key_press(Keycode key) const
		{
			return press() && _attr.press.key == key;
		}

		bool key_release(Keycode key) const
		{
			return release() && _attr.release.key == key;
		}

		template <typename FN>
		void handle_press(FN const &fn) const
		{
			if (press() && _valid(_attr.press.key))
				fn(_attr.press.key, _attr.press.codepoint);
		}

		template <typename FN>
		void handle_repeat(FN const &fn) const
		{
			if (key_press(KEY_UNKNOWN) && _attr.press.codepoint.valid())
				fn(_attr.press.codepoint);
		}

		template <typename FN>
		void handle_release(FN const &fn) const
		{
			if (release() && _valid(_attr.release.key))
				fn(_attr.release.key);
		}

		template <typename FN>
		void handle_relative_motion(FN const &fn) const
		{
			if (relative_motion())
				fn(_attr.rel_motion.x, _attr.rel_motion.y);
		}

		template <typename FN>
		void handle_absolute_motion(FN const &fn) const
		{
			if (absolute_motion())
				fn(_attr.abs_motion.x, _attr.abs_motion.y);
		}

		template <typename FN>
		void handle_wheel(FN const &fn) const
		{
			if (wheel())
				fn(_attr.wheel.x, _attr.wheel.y);
		}

		template <typename FN>
		void handle_touch(FN const &fn) const
		{
			if (touch())
				fn(_attr.touch.id, _attr.touch.x, _attr.touch.y);
		}

		template <typename FN>
		void handle_touch_release(FN const &fn) const
		{
			if (touch_release())
				fn(_attr.touch_release.id);
		}

		template <typename FN>
		void handle_seq_number(FN const &fn) const
		{
			if (seq_number())
				fn(_attr.seq_number);
		}

		template <typename FN>
		void handle_axis(FN const &fn) const
		{
			if (axis())
				fn(_attr.axis.id, _attr.axis.value);
		}

		inline void print(Genode::Output &out) const;
};


void Input::Event::print(Genode::Output &out) const
{
	using Genode::print;
	switch (_type) {
	case INVALID:       print(out, "INVALID"); break;
	case PRESS:         print(out, "PRESS ",   key_name(_attr.press.key),
	                               " ", _attr.press.codepoint.value); break;
	case RELEASE:       print(out, "RELEASE ", key_name(_attr.release.key)); break;
	case REL_MOTION:    print(out, "REL_MOTION ", _xy<int>(_attr.rel_motion)); break;
	case ABS_MOTION:    print(out, "ABS_MOTION ", _xy<int>(_attr.abs_motion)); break;
	case WHEEL:         print(out, "WHEEL ",      _xy<int>(_attr.wheel)); break;
	case FOCUS_ENTER:   print(out, "FOCUS_ENTER");  break;
	case FOCUS_LEAVE:   print(out, "FOCUS_LEAVE");  break;
	case HOVER_LEAVE:   print(out, "HOVER_LEAVE");  break;
	case TOUCH_RELEASE: print(out, "TOUCH_RELEASE ", _attr.touch.id.value); break;
	case TOUCH:         print(out, "TOUCH ", _attr.touch.id.value, " ",
	                                         _xy<float>(_attr.touch)); break;
	case SEQ_NUMBER:    print(out, "SEQ_NUMBER ", _attr.seq_number.value); break;
	case AXIS:          print(out, "AXIS ", unsigned(_attr.axis.id), " ",
	                                        _attr.axis.value); break;
	};
}

#endif /* _INCLUDE__INPUT__EVENT_H_ */
