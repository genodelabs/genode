/*
 * \brief   Platform abstraction
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 *
 * This interface specifies the target-platform-specific functions.
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__PLATFORM_H_
#define _INCLUDE__SCOUT__PLATFORM_H_

#include <base/env.h>
#include <base/attached_dataspace.h>
#include <base/semaphore.h>
#include <timer_session/connection.h>
#include <input_session/input_session.h>
#include <input/event.h>
#include <scout/event.h>
#include <scout/canvas.h>

namespace Scout {

	typedef Genode::Point<> Point;
	typedef Genode::Area<>  Area;
	typedef Genode::Rect<>  Rect;

	class Platform;
}


class Scout::Platform
{
	private:

		/*
		 * Noncopyable
		 */
		Platform(Platform const &);
		Platform &operator = (Platform const &);

		Genode::Env   &_env;
		Event_handler *_event_handler = nullptr;

		int _mx = 0, _my = 0;

		void _handle_event(Event const &ev)
		{
			if (_event_handler)
				_event_handler->handle_event(ev);
		}


		/****************************
		 ** Timer event processing **
		 ****************************/

		Timer::Connection _timer { _env };

		unsigned long _ticks = 0;

		void _handle_timer()
		{
			_ticks = _timer.elapsed_ms();

			Event ev;
			ev.assign(Event::TIMER, _mx, _my, 0);

			_handle_event(ev);
		}

		Genode::Signal_handler<Platform> _timer_handler {
			_env.ep(), *this, &Platform::_handle_timer };


		/****************************
		 ** Input event processing **
		 ****************************/

		Input::Session &_input;

		Genode::Attached_dataspace _input_ds { _env.rm(), _input.dataspace() };

		Input::Event * const _ev_buf = _input_ds.local_addr<Input::Event>();

		bool _event_pending = 0;

		void _handle_input()
		{
			if (_input.pending() == false) return;

			for (int i = 0, num = _input.flush(); i < num; i++) {
				Event ev;
				Input::Event e = _ev_buf[i];

				_event_pending = i + 1 < num;

				e.handle_press([&] (Input::Keycode key, Genode::Codepoint) {
					ev.assign(Event::PRESS, _mx, _my, key); });

				e.handle_release([&] (Input::Keycode key) {
					ev.assign(Event::RELEASE, _mx, _my, key); });

				e.handle_absolute_motion([&] (int x, int y) {
				 	_mx = x; _my = y;
					ev.assign(Event::MOTION, _mx, _my, 0);
				});

				if (ev.type != Event::UNDEFINED)
					_handle_event(ev);
			}
		}

		Genode::Signal_handler<Platform> _input_handler {
			_env.ep(), *this, &Platform::_handle_input};

	public:

		Platform(Genode::Env &env, Input::Session &input)
		: _env(env), _input(input) { }

		/**
		 * Get timer ticks in miilliseconds
		 */
		unsigned long timer_ticks() const { return _ticks; }

		/**
		 * Register event handler
		 */
		void event_handler(Event_handler &handler)
		{
			_event_handler = &handler;

			_timer.sigh(_timer_handler);
			_timer.trigger_periodic(40*1000);

			_input.sigh(_input_handler);
		}

		bool event_pending() const { return _event_pending; }
};

#endif /* _INCLUDE__SCOUT__PLATFORM_H_ */
