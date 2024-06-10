/*
 * \brief  Event session component
 * \author Norman Feske
 * \date   2020-07-14
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_SESSION_H_
#define _EVENT_SESSION_H_

/* Genode includes */
#include <base/session_object.h>
#include <event_session/event_session.h>

/* local includes */
#include <user_state.h>

namespace Nitpicker { class Event_session; }


class Nitpicker::Event_session : public Session_object<Event::Session, Event_session>
{
	public:

		struct Handler : Interface
		{
			virtual void handle_input_events(User_state::Input_batch) = 0;
		};

	private:

		Handler &_handler;

		Constrained_ram_allocator _ram;

		Attached_ram_dataspace _ds;

	public:

		Event_session(Env              &env,
		              Resources  const &resources,
		              Label      const &label,
		              Diag       const &diag,
		              Handler          &handler)
		:
			Session_object(env.ep(), resources, label, diag),
			_handler(handler),
			_ram(env.ram(), _ram_quota_guard(), _cap_quota_guard()),
			_ds(_ram, env.rm(), 4096)
		{ }

		~Event_session() { }


		/*****************************
		 ** Event session interface **
		 *****************************/

		Dataspace_capability dataspace() { return _ds.cap(); }

		void submit_batch(unsigned const count)
		{
			unsigned const max_events = (unsigned)(_ds.size() / sizeof(Input::Event));

			if (count > max_events)
				warning("number of events exceeds dataspace capacity");

			User_state::Input_batch const batch {
				.events = _ds.local_addr<Input::Event>(),
				.count  = min(count, max_events) };

			_handler.handle_input_events(batch);
		}
};

#endif /* _EVENT_SESSION_H_ */
