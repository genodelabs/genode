/*
 * \brief  Event service that prints incoming events
 * \author Norman Feske
 * \date   2020-08-11
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_ram_dataspace.h>
#include <input/event.h>
#include <event_session/event_session.h>
#include <os/static_root.h>

namespace Test {

	using namespace Genode;

	struct Event_session;
	struct Main;
}


struct Test::Event_session : Rpc_object<Event::Session, Event_session>
{
	Attached_ram_dataspace _ds;

	unsigned _event_cnt = 0;
	int      _key_cnt   = 0;

	Dataspace_capability dataspace() { return _ds.cap(); }

	void submit_batch(unsigned count)
	{
		size_t const max_events = _ds.size() / sizeof(Input::Event);

		if (count > max_events)
			warning("number of events exceeds dataspace capacity");

		count = min(count, max_events);

		Input::Event const * const events = _ds.local_addr<Input::Event>();

		for (unsigned i = 0; i < count; i++) {

			Input::Event const &ev = events[i];

			if (ev.press())   ++_key_cnt;
			if (ev.release()) --_key_cnt;

			log("Input event #", _event_cnt++, "\t", ev, "\tkey count: ", _key_cnt);
		}
	}

	Event_session(Env &env) : _ds(env.ram(), env.rm(), 4096) { }
};


/******************
 ** Main program **
 ******************/

struct Test::Main
{
	Env &_env;

	Event_session _event_session { _env };

	/*
	 * Attach root interfaces to the entry point
	 */
	Static_root<Event::Session> _event_root { _env.ep().manage(_event_session) };

	Main(Env &env)
	:
		_env(env)
	{
		_env.parent().announce(env.ep().manage(_event_root));
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

