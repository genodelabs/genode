/*
 * \brief  Frontend of input service
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INPUT__COMPONENT_H_
#define _INCLUDE__INPUT__COMPONENT_H_

#include <base/env.h>
#include <base/rpc_server.h>
#include <os/attached_ram_dataspace.h>
#include <os/ring_buffer.h>
#include <root/component.h>
#include <input_session/input_session.h>
#include <input/event_queue.h>

namespace Input { class Session_component; }


class Input::Session_component : public Genode::Rpc_object<Input::Session>
{
	private:

		Genode::Attached_ram_dataspace _ds { Genode::env()->ram_session(),
		                                     Event_queue::QUEUE_SIZE*sizeof(Input::Event) };

		Event_queue _event_queue;

	public:

		/**
		 * Return reference to event queue of the session
		 */
		Event_queue &event_queue() { return _event_queue; }

		/**
		 * Submit input event to event queue
		 */
		void submit(Input::Event event)
		{
			try {
				_event_queue.add(event);
			} catch (Input::Event_queue::Overflow) {
				PWRN("input overflow - resetting queue");
				_event_queue.reset();
			}
		}


		/******************************
		 ** Input::Session interface **
		 ******************************/

		Genode::Dataspace_capability dataspace() override { return _ds.cap(); }

		bool pending() const override { return !_event_queue.empty(); }

		/*
		 * \deprecated  use 'pending' instead
		 */
		bool is_pending() const { return pending(); }

		int flush() override
		{
			Input::Event *dst = _ds.local_addr<Input::Event>();
			
			unsigned cnt = 0;
			for (; cnt < Event_queue::QUEUE_SIZE && !_event_queue.empty(); cnt++)
				*dst++ = _event_queue.get();

			return cnt;
		}

		void sigh(Genode::Signal_context_capability sigh) override
		{
			_event_queue.sigh(sigh);
		}
};

#endif /* _INCLUDE__INPUT__COMPONENT_H_ */
