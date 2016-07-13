/*
 * \brief   Platform abstraction
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 *
 * This interface specifies the target-platform-specific functions.
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SCOUT__PLATFORM_H_
#define _INCLUDE__SCOUT__PLATFORM_H_

#include <base/env.h>
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


inline void *operator new(Genode::size_t size)
{
	using Genode::env;
	void *addr = env()->heap()->alloc(size);
	if (!addr) {
		Genode::error("env()->heap() has consumed ", env()->heap()->consumed());
		Genode::error("env()->ram_session()->quota = ", env()->ram_session()->quota());
		throw Genode::Allocator::Out_of_memory();
	}
	return addr;
}


class Scout::Platform
{
	private:

		/*****************
		 ** Event queue **
		 *****************/
		
		class Event_queue
		{
			private:

				static const int queue_size = 1024;

				int               _head;
				int               _tail;
				Genode::Semaphore _sem;
				Genode::Lock      _head_lock;  /* synchronize add */

				Event _queue[queue_size];

			public:

				/**
				 * Constructor
				 */
				Event_queue(): _head(0), _tail(0)
				{
					Genode::memset(_queue, 0, sizeof(_queue));
				}

				void add(Event ev)
				{
					Genode::Lock::Guard lock_guard(_head_lock);

					if ((_head + 1)%queue_size != _tail) {

						_queue[_head] = ev;
						_head = (_head + 1)%queue_size;
						_sem.up();
					}
				}

				Event get()
				{
					_sem.down();
					Event ev = _queue[_tail];
					_tail = (_tail + 1)%queue_size;
					return ev;
				}

				int pending() const { return _head != _tail; }

		} _event_queue;

		/******************
		 ** Timer thread **
		 ******************/

		class Timer_thread : public Genode::Thread_deprecated<4096>
		{
			private:

				Timer::Connection _timer;
				Input::Session   &_input;
				Input::Event     *_ev_buf = { Genode::env()->rm_session()->attach(_input.dataspace()) };
				Event_queue      &_event_queue;
				int               _mx, _my;
				unsigned long     _ticks = { 0 };

				void _import_events()
				{
					if (_input.pending() == false) return;

					for (int i = 0, num = _input.flush(); i < num; i++)
					{
						Event ev;
						Input::Event e = _ev_buf[i];

						if (e.type() == Input::Event::RELEASE
						 || e.type() == Input::Event::PRESS) {
							_mx = e.ax();
							_my = e.ay();
							ev.assign(e.type() == Input::Event::PRESS ? Event::PRESS : Event::RELEASE,
							          e.ax(), e.ay(), e.code());
							_event_queue.add(ev);
						}

						if (e.type() == Input::Event::MOTION) {
							_mx = e.ax();
							_my = e.ay();
							ev.assign(Event::MOTION, e.ax(), e.ay(), e.code());
							_event_queue.add(ev);
						}
					}
				}

			public:

				/**
				 * Constructor
				 *
				 * Start thread immediately on construction.
				 */
				Timer_thread(Input::Session &input, Event_queue &event_queue)
				: Thread_deprecated("timer"), _input(input), _event_queue(event_queue)
				{ start(); }

				void entry()
				{
					while (1) {
						Event ev;
						ev.assign(Event::TIMER, _mx, _my, 0);
						_event_queue.add(ev);

						_import_events();

						_timer.msleep(10);
						_ticks += 10;
					}
				}

				unsigned long ticks() const { return _ticks; }
		} _timer_thread;

	public:

		Platform(Input::Session &input) : _timer_thread(input, _event_queue) { }

		/**
		 * Get timer ticks in miilliseconds
		 */
		unsigned long timer_ticks() const { return _timer_thread.ticks(); }

		/**
		 * Return true if an event is pending
		 */
		bool event_pending() const { return _event_queue.pending(); }

		/**
		 * Request event
		 *
		 * \param e  destination where to store event information.
		 *
		 * If there is no event pending, this function blocks
		 * until there is an event to deliver.
		 */
		Event get_event() { return _event_queue.get(); }
};

#endif /* _INCLUDE__SCOUT__PLATFORM_H_ */
