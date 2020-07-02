/*
 * \brief  Service for connecting an input client with an event client
 * \author Norman Feske
 * \date   2020-07-02
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <event_session/event_session.h>
#include <input/component.h>
#include <input/event.h>
#include <os/static_root.h>
#include <base/component.h>
#include <base/attached_ram_dataspace.h>

namespace Input_event_bridge {

	using namespace Genode;

	struct Event_session;
	struct Main;
}


struct Input_event_bridge::Event_session : Rpc_object<Event::Session, Event_session>
{
	Attached_ram_dataspace _ds;

	Input::Session_component &_input_session;

	Dataspace_capability dataspace() { return _ds.cap(); }

	void submit_batch(unsigned count)
	{
		size_t const max_events = _ds.size() / sizeof(Input::Event);

		if (count > max_events)
			warning("number of events exceeds dataspace capacity");

		count = min(count, max_events);

		Input::Event const * const events = _ds.local_addr<Input::Event>();

		for (unsigned i = 0; i < count; i++)
			_input_session.submit(events[i]);
	}

	Event_session(Env &env, Input::Session_component &input_session)
	:
		_ds(env.ram(), env.rm(), 4096), _input_session(input_session)
	{ }
};


/******************
 ** Main program **
 ******************/

struct Input_event_bridge::Main
{
	Env &_env;

	Attached_ram_dataspace _ds { _env.ram(), _env.rm(), 4096 };

	Input::Session_component _input_session { _env, _env.ram() };
	Event_session            _event_session { _env, _input_session };

	/*
	 * Attach root interfaces to the entry point
	 */
	Static_root<Input::Session> _input_root { _env.ep().manage(_input_session) };
	Static_root<Event::Session> _event_root { _env.ep().manage(_event_session) };

	/**
	 * Constructor
	 */
	Main(Env &env) : _env(env)
	{
		_input_session.event_queue().enabled(true);

		_env.parent().announce(env.ep().manage(_input_root));
		_env.parent().announce(env.ep().manage(_event_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Input_event_bridge::Main inst(env);
}
