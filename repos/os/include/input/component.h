/*
 * \brief  Frontend of input service
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__INPUT__COMPONENT_H_
#define _INCLUDE__INPUT__COMPONENT_H_

#include <base/env.h>
#include <base/rpc_server.h>
#include <base/attached_ram_dataspace.h>
#include <os/ring_buffer.h>
#include <root/component.h>
#include <input_session/input_session.h>
#include <input/event_queue.h>

namespace Input { class Session_component; }


class Input::Session_component : public Rpc_object<Input::Session>
{
	public:

		struct Action : Interface
		{
			virtual void exclusive_input_requested(bool) = 0;
		};

	private:

		Entrypoint &_ep;
		Action     &_action;

		Attached_ram_dataspace _ds;

		Event_queue _event_queue { };

	public:

		/**
		 * Constructor
		 *
		 * \param ram  allocator for the shared-memory input buffer
		 */
		Session_component(Entrypoint &ep, Ram_allocator &ram,
		                  Env::Local_rm &rm, Action &action)
		:
			_ep(ep), _action(action),
			_ds(ram, rm, Event_queue::QUEUE_SIZE*sizeof(Input::Event))
		{
			_ep.manage(*this);
		}

		~Session_component() { _ep.dissolve(*this); }

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
				warning("input overflow - resetting queue");
				_event_queue.reset();
			}
		}


		/******************************
		 ** Input::Session interface **
		 ******************************/

		Dataspace_capability dataspace() override { return _ds.cap(); }

		bool pending() const override { return !_event_queue.empty(); }

		int flush() override
		{
			Input::Event *dst = _ds.local_addr<Input::Event>();
			
			unsigned cnt = 0;
			for (; cnt < Event_queue::QUEUE_SIZE && !_event_queue.empty(); cnt++)
				*dst++ = _event_queue.get();

			return cnt;
		}

		void sigh(Signal_context_capability sigh) override
		{
			_event_queue.sigh(sigh);
		}

		void exclusive(bool enabled) override
		{
			_action.exclusive_input_requested(enabled);
		}
};

#endif /* _INCLUDE__INPUT__COMPONENT_H_ */
