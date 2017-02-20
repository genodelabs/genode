/*
 * \brief  Input event queue
 * \author Norman Feske
 * \date   2007-10-08
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_QUEUE_H_
#define _EVENT_QUEUE_H_

#include <base/signal.h>
#include <input/event.h>
#include <os/ring_buffer.h>

namespace Input { class Event_queue; };


class Input::Event_queue
{
	public:

		/**
		 * Input event queue
		 *
		 * We expect the client to fetch events circa each 10ms. The PS/2 driver
		 * queues up to 255 events, which should be enough. Normally, PS/2
		 * generates not more than 16Kbit/s, which would correspond to ca. 66 mouse
		 * events per 10ms.
		 */
		enum { QUEUE_SIZE = 512U };

		typedef Genode::Ring_buffer<Input::Event, QUEUE_SIZE> Ring_buffer;

	private:

		Ring_buffer _queue;

		bool _enabled = false;

		Genode::Signal_context_capability _sigh;

	public:

		typedef Ring_buffer::Overflow Overflow;

		void enabled(bool enabled) { _enabled = enabled; }

		bool enabled() const { return _enabled; }

		void sigh(Genode::Signal_context_capability sigh) { _sigh = sigh; }

		void submit_signal()
		{
			if (_sigh.valid())
				Genode::Signal_transmitter(_sigh).submit();
		}

		/**
		 * \throw Overflow
		 */
		void add(Input::Event ev, bool submit_signal_immediately = true)
		{
			if (!_enabled)
				return;

			_queue.add(ev);

			if (submit_signal_immediately)
				submit_signal();
		}

		Input::Event get() { return _queue.get(); }

		bool empty() const { return _queue.empty(); }

		int avail_capacity() const { return _queue.avail_capacity(); }

		void reset() { _queue.reset(); }
};

#endif /* _EVENT_QUEUE_H_ */
