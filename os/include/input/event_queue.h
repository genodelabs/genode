/*
 * \brief  Input event queue
 * \author Norman Feske
 * \date   2007-10-08
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EVENT_QUEUE_H_
#define _EVENT_QUEUE_H_

#include <base/printf.h>
#include <input/event.h>
#include <os/ring_buffer.h>

/**
 * Input event queue
 *
 * We expect the client to fetch events circa each 10ms. The PS/2 driver queues
 * up to 255 events, which should be enough. Normally, PS/2 generates not more
 * than 16Kbit/s, which would correspond to ca. 66 mouse events per 10ms.
 */
class Event_queue
{
	private:

		bool                           _enabled;
		Ring_buffer<Input::Event, 512> _ev_queue;

	public:

		Event_queue() : _enabled(false), _ev_queue() { }

		void enable()  { _enabled = true; }
		void disable() { _enabled = false; }

		void add(Input::Event e)
		{
			if (!_enabled) return;

			try {
				_ev_queue.add(e);
			} catch (Ring_buffer<Input::Event, 512>::Overflow) {
				PWRN("event buffer overflow");
			}
		}

		Input::Event get()
		{
			if (_enabled)
				return _ev_queue.get();
			else
				return Input::Event();
		}

		bool empty()
		{
			if (_enabled)
				return _ev_queue.empty();
			else
				return true;
		}
};

#endif /* _EVENT_QUEUE_H_ */
