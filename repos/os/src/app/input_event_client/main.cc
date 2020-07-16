/*
 * \brief  Application for connecting an input server with an event server
 * \author Norman Feske
 * \date   2020-07-16
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <event_session/connection.h>
#include <input_session/connection.h>
#include <base/component.h>

namespace Input_event_client {
	using namespace Genode;
	struct Main;
}


struct Input_event_client::Main
{
	Env &_env;

	Input::Connection _input { _env };
	Event::Connection _event { _env };

	Signal_handler<Main> _input_handler { _env.ep(), *this, &Main::_handle_input };

	void _handle_input()
	{
		_event.with_batch([&] (Event::Session_client::Batch &batch) {
			_input.for_each_event([&] (Input::Event const &event) {
				batch.submit(event); }); });
	}

	Main(Env &env) : _env(env) { _input.sigh(_input_handler); }
};


void Component::construct(Genode::Env &env)
{
	static Input_event_client::Main inst(env);
}
