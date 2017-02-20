/*
 * \brief  Input root component
 * \author Norman Feske
 * \date   2014-05-31
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INPUT__ROOT_H_
#define _INPUT__ROOT_H_

/* Genode includes */
#include <os/static_root.h>
#include <input/component.h>

namespace Input { class Root_component; }


/*
 * This input root component tracks if the session has been opened. If a client
 * is connected, the 'Event_queue::enabled' gets enabled. This is useful to
 * omit the enqueuing of input events into the event queue before any client is
 * interested in receiving input events. If we would not drop such early input
 * events, the queue might overflow when input events are generated at boot
 * times.
 */
class Input::Root_component : public Genode::Static_root<Input::Session>
{
	private:

		Genode::Rpc_entrypoint   &_ep;
		Input::Session_component &_session;

	public:

		Root_component(Genode::Rpc_entrypoint &ep, Input::Session_component &session)
		:
			Static_root<Input::Session>(ep.manage(&session)),
			_ep(ep), _session(session)
		{ }

		~Root_component()
		{
			_ep.dissolve(&_session);
		}

		Genode::Capability<Genode::Session>
		session(Genode::Root::Session_args const &args,
    	        Genode::Affinity           const &affinity) override
		{
			if (_session.event_queue().enabled())
				throw Root::Unavailable();

			_session.event_queue().enabled(true);

			return Static_root<Input::Session>::session(args, affinity);
		}

		void close(Genode::Capability<Session>)
		{
			_session.event_queue().enabled(false);
		}
};

#endif /* _INPUT__ROOT_H_ */
